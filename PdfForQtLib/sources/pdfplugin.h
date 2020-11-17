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

#ifndef PDFPLUGIN_H
#define PDFPLUGIN_H

#include "pdfdocument.h"

#include <QObject>
#include <QJsonObject>

namespace pdf
{
class PDFWidget;

struct PDFFORQTLIBSHARED_EXPORT PDFPluginInfo
{
    QString name;
    QString author;
    QString version;
    QString license;
    QString description;

    static PDFPluginInfo loadFromJson(const QJsonObject* json);
};
using PDFPluginInfos = std::vector<PDFPluginInfo>;

class PDFFORQTLIBSHARED_EXPORT PDFPlugin : public QObject
{
    Q_OBJECT

public:
    explicit PDFPlugin(QObject* parent);

    virtual void setWidget(PDFWidget* widget);
    virtual void setDocument(const PDFModifiedDocument& document);
};

}   // namespace pdf

#endif // PDFPLUGIN_H