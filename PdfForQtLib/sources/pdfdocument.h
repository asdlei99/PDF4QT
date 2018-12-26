//    Copyright (C) 2018 Jakub Melka
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


#ifndef PDFDOCUMENT_H
#define PDFDOCUMENT_H

#include "pdfglobal.h"
#include "pdfobject.h"
#include "pdfcatalog.h"

#include <QtCore>
#include <QDateTime>

namespace pdf
{
class PDFDocument;

/// Storage for objects. This class is not thread safe for writing (calling non-const functions). Caller must ensure
/// locking, if this object is used from multiple threads. Calling const functions should be thread safe.
class PDFObjectStorage
{
public:
    constexpr inline PDFObjectStorage() = default;

    constexpr inline PDFObjectStorage(const PDFObjectStorage&) = default;
    constexpr inline PDFObjectStorage(PDFObjectStorage&&) = default;

    constexpr inline PDFObjectStorage& operator=(const PDFObjectStorage&) = default;
    constexpr inline PDFObjectStorage& operator=(PDFObjectStorage&&) = default;

    struct Entry
    {
        constexpr inline explicit Entry() = default;
        inline explicit Entry(PDFInteger generation, PDFObject object) : generation(generation), object(std::move(object)) { }

        PDFInteger generation = 0;
        PDFObject object;
    };

    using PDFObjects = std::vector<Entry>;

    explicit PDFObjectStorage(PDFObjects&& objects, PDFObject&& trailerDictionary) :
        m_objects(std::move(objects)),
        m_trailerDictionary(std::move(trailerDictionary))
    {

    }

    /// Returns object from the object storage. If invalid reference is passed,
    /// then null object is returned (no exception is thrown).
    const PDFObject& getObject(PDFObjectReference reference) const;

    /// Returns array of objects stored in this storage
    const PDFObjects& getObjects() const { return m_objects; }

    /// Returns trailer dictionary
    const PDFObject& getTrailerDictionary() const { return m_trailerDictionary; }

private:
    PDFObjects m_objects;
    PDFObject m_trailerDictionary;
};

/// Loads data from the object contained in the PDF document, such as integers,
/// bools, ... This object has two sets of functions - first one with default values,
/// then if object with valid data is not found, default value is used, and second one,
/// without default value, if valid data are not found, then exception is thrown.
/// This class uses Decorator design pattern.
class PDFDocumentDataLoaderDecorator
{
public:
    inline explicit PDFDocumentDataLoaderDecorator(const PDFDocument* document) : m_document(document) { }
    inline ~PDFDocumentDataLoaderDecorator() = default;

    /// Reads an integer from the object, if it is possible.
    /// \param object Object, can be an indirect reference to object (it is dereferenced)
    /// \param defaultValue Default value
    PDFInteger readInteger(const PDFObject& object, PDFInteger defaultValue) const;

    /// Reads a text string from the object, if it is possible.
    /// \param object Object, can be an indirect reference to object (it is dereferenced)
    /// \param defaultValue Default value
    QString readTextString(const PDFObject& object, const QString& defaultValue) const;

    /// Reads a rectangle from the object, if it is possible.
    /// \param object Object, can be an indirect reference to object (it is dereferenced)
    /// \param defaultValue Default value
    QRectF readRectangle(const PDFObject& object, const QRectF& defaultValue) const;

    /// Reads enum from name object, if it is possible.
    /// \param object Object, can be an indirect reference to object (it is dereferenced)
    /// \param begin Begin of the enum search array
    /// \param end End of the enum search array
    /// \param default value Default value
    template<typename Enum, typename Iterator>
    Enum readEnumByName(const PDFObject& object, Iterator begin, Iterator end, Enum defaultValue) const
    {
        const PDFObject& dereferencedObject = m_document->getObject(object);
        if (dereferencedObject.isName())
        {
            QByteArray name = dereferencedObject.getString();

            for (Iterator it = begin; it != end; ++it)
            {
                if (name == (*it).first)
                {
                    return (*it).second;
                }
            }
        }

        return defaultValue;
    }

private:
    const PDFDocument* m_document;
};

/// PDF document main class.
class PDFDocument
{
    Q_DECLARE_TR_FUNCTIONS(pdf::PDFDocument)

public:
    explicit PDFDocument() = default;

    const PDFObjectStorage& getStorage() const { return m_pdfObjectStorage; }

    /// Info about the document. Title, Author, Keywords...
    struct Info
    {
        /// Indicates, that document was modified that it includes trapping information.
        /// See PDF Reference 1.7, Section 10.10.5 "Trapping Support".
        enum class Trapped
        {
            True,       ///< Fully trapped
            False,      ///< Not yet trapped
            Unknown     ///< Either unknown, or it has been trapped partly, not fully
        };

        QString title;
        QString author;
        QString subject;
        QString keywords;
        QString creator;
        QString producer;
        QDateTime creationDate;
        QDateTime modifiedDate;
        Trapped trapped = Trapped::Unknown;
    };

    /// Returns info about the document (title, author, etc.)
    const Info* getInfo() const { return &m_info; }

    /// If object is reference, the dereference attempt is performed
    /// and object is returned. If it is not a reference, then self
    /// is returned. If dereference attempt fails, then null object
    /// is returned (no exception is thrown).
    const PDFObject& getObject(const PDFObject& object) const;

private:
    friend class PDFDocumentReader;

    explicit PDFDocument(PDFObjectStorage&& storage) :
        m_pdfObjectStorage(std::move(storage))
    {
        init();
    }

    /// Initialize data based on object in the storage.
    /// Can throw exception if error is detected.
    void init();

    /// Initialize the document info from the trailer dictionary.
    /// If document info is not present, then default document
    /// info is used. If error is detected, exception is thrown.
    void initInfo();

    /// Storage of objects
    PDFObjectStorage m_pdfObjectStorage;

    /// Info about the PDF document
    Info m_info;

    /// Catalog object
    PDFCatalog m_catalog;
};

// Implementation

inline
const PDFObject& PDFDocument::getObject(const PDFObject& object) const
{
    if (object.isReference())
    {
        // Try to dereference the object
        return m_pdfObjectStorage.getObject(object.getReference());
    }

    return object;
}

}   // namespace pdf

#endif // PDFDOCUMENT_H
