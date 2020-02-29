//    Copyright (C) 2020 Jakub Melka
//
//    This file is part of PdfForQt.
//
//    PdfForQt is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    PdfForQt is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with PDFForQt.  If not, see <https://www.gnu.org/licenses/>.

#include "pdfsnapper.h"
#include "pdfcompiler.h"
#include "pdfwidgetutils.h"
#include "pdfdrawspacecontroller.h"

#include <QPainter>

namespace pdf
{

void PDFSnapInfo::addPageMediaBox(const QRectF& mediaBox)
{
    QPointF tl = mediaBox.topLeft();
    QPointF tr = mediaBox.topRight();
    QPointF bl = mediaBox.bottomLeft();
    QPointF br = mediaBox.bottomRight();
    QPointF center = mediaBox.center();

    m_snapPoints.insert(m_snapPoints.cend(), {
                            SnapPoint(SnapType::PageCorner, tl ),
                            SnapPoint(SnapType::PageCorner, tr ),
                            SnapPoint(SnapType::PageCorner, bl ),
                            SnapPoint(SnapType::PageCorner, br ),
                            SnapPoint(SnapType::PageCenter, center)
                        });

    addLine(tl, tr);
    addLine(tr, br);
    addLine(br, bl);
    addLine(tl, bl);
}

void PDFSnapInfo::addImage(const std::array<QPointF, 5>& points)
{
    m_snapPoints.insert(m_snapPoints.cend(), {
                            SnapPoint(SnapType::ImageCorner, points[0]),
                            SnapPoint(SnapType::ImageCorner, points[1]),
                            SnapPoint(SnapType::ImageCorner, points[2]),
                            SnapPoint(SnapType::ImageCorner, points[3]),
                            SnapPoint(SnapType::ImageCenter, points[4])
                        });

    for (size_t i = 0; i < 4; ++i)
    {
        addLine(points[i], points[(i + 1) % 4]);
    }
}

void PDFSnapInfo::addLine(const QPointF& start, const QPointF& end)
{
    QLineF line(start, end);
    m_snapPoints.emplace_back(SnapType::LineCenter, line.center());
    m_snapLines.emplace_back(line);
}

PDFSnapper::PDFSnapper()
{

}

void PDFSnapper::drawSnapPoints(QPainter* painter) const
{
    Q_ASSERT(painter);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen = painter->pen();
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(m_snapPointPixelSize);

    for (const ViewportSnapPoint& snapPoint : m_snapPoints)
    {
        if (!isSnappingAllowed(snapPoint.pageIndex))
        {
            // We are drawing only snap points, which are on current page
            continue;
        }

        QColor color = pen.color();
        QColor newColor = color;
        switch (snapPoint.type)
        {
            case SnapType::PageCorner:
                newColor = Qt::blue;
                break;

            case SnapType::GeneratedLineProjection:
                newColor = Qt::green;
                break;

            default:
                newColor = Qt::red;
                break;
        }

        if (color != newColor)
        {
            pen.setColor(newColor);
            painter->setPen(pen);
        }

        QPoint point = snapPoint.viewportPoint.toPoint();
        painter->drawPoint(point);
    }

    if (isSnapped())
    {
        pen.setColor(Qt::yellow);
        painter->setPen(pen);
        QPoint point = m_snappedPoint->viewportPoint.toPoint();
        painter->drawPoint(point);
    }

    painter->restore();
}

bool PDFSnapper::isSnappingAllowed(PDFInteger pageIndex) const
{
    return (m_currentPage == -1) || (m_currentPage == pageIndex);
}

void PDFSnapper::updateSnappedPoint(const QPointF& mousePoint)
{
    m_snappedPoint = std::nullopt;
    m_mousePoint = mousePoint;

    // Iterate trough all points, check, if some satisfies condition
    const PDFReal toleranceSquared = m_snapPointTolerance * m_snapPointTolerance;
    for (const ViewportSnapPoint& snapPoint : m_snapPoints)
    {
        QPointF difference = mousePoint - snapPoint.viewportPoint;
        PDFReal distanceSquared = QPointF::dotProduct(difference, difference);
        if (distanceSquared < toleranceSquared && isSnappingAllowed(snapPoint.pageIndex))
        {
            m_snappedPoint = snapPoint;
            return;
        }
    }
}

void PDFSnapper::buildSnapPoints(const PDFWidgetSnapshot& snapshot)
{
    // First, clear all snap points
    m_snapPoints.clear();

    // Second, create snapping points from snapshot
    for (const PDFWidgetSnapshot::SnapshotItem& item : snapshot.items)
    {
        if (!item.compiledPage)
        {
            continue;
        }

        const PDFSnapInfo* info = item.compiledPage->getSnapInfo();
        for (const PDFSnapInfo::SnapPoint& snapPoint : info->getSnapPoints())
        {
            ViewportSnapPoint viewportSnapPoint;
            viewportSnapPoint.type = snapPoint.type;
            viewportSnapPoint.point = snapPoint.point;
            viewportSnapPoint.pageIndex = item.pageIndex;
            viewportSnapPoint.viewportPoint = item.pageToDeviceMatrix.map(snapPoint.point);
            m_snapPoints.push_back(qMove(viewportSnapPoint));
        }

        // Fill line projections snap points
        if (m_currentPage == item.pageIndex && m_referencePoint.has_value())
        {
            QPointF referencePoint = *m_referencePoint;
            for (const QLineF& line : info->getLines())
            {
                // Project point onto line.
                const qreal lineLength = line.length();
                QPointF vector = referencePoint - line.p1();
                QPointF tangentVector = (line.p2() - line.p1()) / lineLength;
                const qreal absoluteParameter = QPointF::dotProduct(vector, tangentVector);
                if (absoluteParameter >= 0 && absoluteParameter <= lineLength)
                {
                    QPointF projectedSnapPoint = line.pointAt(absoluteParameter / lineLength);
                    const PDFReal tolerance = lineLength * 0.01;
                    const PDFReal squaredTolerance = tolerance * tolerance;

                    // Test, if projected snap point is not already present in snap points
                    auto testSamePoints = [projectedSnapPoint, squaredTolerance](const ViewportSnapPoint& testedSnapPoint)
                    {
                        return isFuzzyComparedPointsSame(projectedSnapPoint, testedSnapPoint.point, squaredTolerance);
                    };
                    if (m_snapPoints.cend() == std::find_if(m_snapPoints.cbegin(), m_snapPoints.cend(), testSamePoints))
                    {
                        ViewportSnapPoint viewportSnapPoint;
                        viewportSnapPoint.type = SnapType::GeneratedLineProjection;
                        viewportSnapPoint.point = projectedSnapPoint;
                        viewportSnapPoint.pageIndex = item.pageIndex;
                        viewportSnapPoint.viewportPoint = item.pageToDeviceMatrix.map(projectedSnapPoint);
                        m_snapPoints.push_back(qMove(viewportSnapPoint));
                    }
                }
            }
        }
    }

    // Third, update snap shot position
    updateSnappedPoint(m_mousePoint);
}

int PDFSnapper::getSnapPointTolerance() const
{
    return m_snapPointTolerance;
}

void PDFSnapper::setSnapPointTolerance(int snapPointTolerance)
{
    m_snapPointTolerance = snapPointTolerance;
}

QPointF PDFSnapper::getSnappedPoint() const
{
    if (isSnapped())
    {
        return m_snappedPoint->viewportPoint;
    }

    return m_mousePoint;
}

void PDFSnapper::setReferencePoint(PDFInteger pageIndex, QPointF pagePoint)
{
    m_currentPage = pageIndex;
    m_referencePoint = pagePoint;
}

void PDFSnapper::clearReferencePoint()
{
    m_currentPage = -1;
    m_referencePoint = std::nullopt;
}

}   // namespace pdf