//    Copyright (C) 2021 Jakub Melka
//
//    This file is part of PDF4QT.
//
//    PDF4QT is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    with the written consent of the copyright owner, any later version.
//
//    PDF4QT is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public License
//    along with PDF4QT.  If not, see <https://www.gnu.org/licenses/>.

#include "differencesdockwidget.h"
#include "ui_differencesdockwidget.h"

#include "pdfdiff.h"
#include "pdfwidgetutils.h"

#include <QTreeWidgetItem>

namespace pdfdocdiff
{

DifferenceItemDelegate::DifferenceItemDelegate(QObject* parent) :
    BaseClass(parent)
{

}

void DifferenceItemDelegate::paint(QPainter* painter,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    BaseClass::paint(painter, option, index);
}

QSize DifferenceItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    if (!option.rect.isValid())
    {
        // Jakub Melka: Why this? We need to use text wrapping. Unfortunately,
        // standard delegate needs correct text rectangle (at least rectangle width),
        // for word wrap calculation. So we must manually calculate rectangle width.
        // Of course, we cant use visualRect of the tree widget, because of cyclical
        // dependence.
        QStyleOptionViewItem adjustedOption = option;
        const QTreeWidget* treeWidget = qobject_cast<const QTreeWidget*>(option.widget);
        int xOffset = treeWidget->columnViewportPosition(index.column());
        int height = option.fontMetrics.lineSpacing();
        int yOffset = 0;
        int width = treeWidget->columnWidth(index.column());

        int level = treeWidget->rootIsDecorated() ? 1 : 0;
        QModelIndex currentIndex = index.parent();
        while (currentIndex.isValid())
        {
            ++level;
            currentIndex = currentIndex.parent();
        }

        xOffset += level * treeWidget->indentation();
        adjustedOption.rect = QRect(xOffset, yOffset, width - xOffset, height);
        return BaseClass::sizeHint(adjustedOption, index);
    }

    return BaseClass::sizeHint(option, index);
}

DifferencesDockWidget::DifferencesDockWidget(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::DifferencesDockWidget),
    m_diffResult(nullptr),
    m_diffNavigator(nullptr)
{
    ui->setupUi(this);

    ui->differencesTreeWidget->setItemDelegate(new DifferenceItemDelegate(this));

    setMinimumWidth(pdf::PDFWidgetUtils::scaleDPI_x(this, 120));
}

DifferencesDockWidget::~DifferencesDockWidget()
{
    delete ui;
}

void DifferencesDockWidget::update()
{
    ui->differencesTreeWidget->clear();

    QList<QTreeWidgetItem*> topItems;

    QLocale locale;

    if (m_diffResult && !m_diffResult->isSame())
    {
        const size_t differenceCount = m_diffResult->getDifferencesCount();

        pdf::PDFInteger lastLeftPageIndex = -1;
        pdf::PDFInteger lastRightPageIndex = -1;

        for (size_t i = 0; i < differenceCount; ++i)
        {
            pdf::PDFInteger pageIndexLeft = m_diffResult->getLeftPage(i);
            pdf::PDFInteger pageIndexRight = m_diffResult->getRightPage(i);

            if (lastLeftPageIndex != pageIndexLeft ||
                lastRightPageIndex != pageIndexRight ||
                topItems.empty())
            {
                // Create new top level item
                QStringList captionParts;
                captionParts << QString("#%1:").arg(topItems.size() + 1);

                if (pageIndexLeft == pageIndexRight)
                {
                    captionParts << tr("Page %1").arg(locale.toString(pageIndexLeft + 1));
                }
                else
                {
                    if (pageIndexLeft != -1)
                    {
                        captionParts << tr("Left %1").arg(locale.toString(pageIndexLeft + 1));
                    }

                    if (pageIndexRight != -1)
                    {
                        captionParts << tr("Right %1").arg(locale.toString(pageIndexRight + 1));
                    }
                }

                QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << captionParts.join(" "));
                topItems << item;

                lastLeftPageIndex = pageIndexLeft;
                lastRightPageIndex = pageIndexRight;
            }

            Q_ASSERT(!topItems.isEmpty());
            QTreeWidgetItem* parent = topItems.back();

            QTreeWidgetItem* item = new QTreeWidgetItem(parent, QStringList() << m_diffResult->getMessage(i));
            item->setData(0, Qt::UserRole, i);
        }
    }

    ui->differencesTreeWidget->addTopLevelItems(topItems);
    ui->differencesTreeWidget->expandAll();
}

void DifferencesDockWidget::setDiffResult(pdf::PDFDiffResult* diffResult)
{
    m_diffResult = diffResult;
}

void DifferencesDockWidget::setDiffNavigator(pdf::PDFDiffResultNavigator* diffNavigator)
{
    m_diffNavigator = diffNavigator;
}

}   // namespace pdfdocdiff
