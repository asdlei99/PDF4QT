//    Copyright (C) 2022 Jakub Melka
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

#include "pdf3dsceneprocessor.h"
#include "pdf3d_u3d.h"
#include "pdfdbgheap.h"

#include <QPainter>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QSpotLight>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QPointSize>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QCullFace>
#include <Qt3DRender/QSortPolicy>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QAlphaTest>
#include <Qt3DExtras/QDiffuseSpecularMaterial>
#include <Qt3DExtras/QPhongAlphaMaterial>
#include <Qt3DExtras/QPerVertexColorMaterial>
#include <Qt3DExtras/QTextureMaterial>

namespace pdfviewer
{

/*
        English name                    Acrobat czech translation

        Solid                           Plny
        SolidWireframe                  Plny dratovy model
        Transparent                     Pruhledne
        TransparentWireframe            Pruhledny dratovy model
        BoundingBox                     Ohranicovaci ramecek
        TransparentBoundingBox          Pruhledny ohranicovaci ramecek
        TransparentBoundingBoxOutline   Obrys pruhledneho ohranicovaciho ramecku
        Wireframe                       Dratovy model
        ShadedWireframe                 Stinovany dratovy model
        HiddenWireframe                 Skryty dratovy model
        Vertices                        Vrcholy
        ShadedVertices                  Stinovane vrcholy
        Illustration                    Ilustrace
        SolidOutline                    Plny obrys
        ShadedIllustration              Stinovana ilustrace
*/

class PDF3DTextureImage : public Qt3DRender::QPaintedTextureImage
{
    using BaseClass = Qt3DRender::QPaintedTextureImage;
public:
    explicit PDF3DTextureImage(QImage image, Qt3DCore::QNode* parent) :
        BaseClass(parent),
        m_image(std::move(image))
    {

    }

    virtual ~PDF3DTextureImage() override
    {

    }

protected:
    virtual void paint(QPainter* painter) override;

private:
    QImage m_image;
};

void PDF3DTextureImage::paint(QPainter* painter)
{
    painter->setViewport(0, height(), width(), -height());
    painter->drawImage(QRectF(QPointF(0, 0), size()), m_image);
}

PDF3DSceneProcessor::Scene PDF3DSceneProcessor::createScene(const pdf::u3d::PDF3D_U3D* sceneData)
{
    Scene scene;

    // Clear processed nodes
    m_processedNodes.clear();
    m_sceneData = sceneData;

    scene.sceneRoot = createNode(sceneData->getNode(m_sceneRoot));

    return scene;
}

PDF3DSceneProcessor::SceneMode PDF3DSceneProcessor::getMode() const
{
    return m_mode;
}

void PDF3DSceneProcessor::setMode(SceneMode newMode)
{
    m_mode = newMode;
}

const QColor& PDF3DSceneProcessor::getAuxiliaryColor() const
{
    return m_auxiliaryColor;
}

void PDF3DSceneProcessor::setAuxiliaryColor(const QColor& newAuxiliaryColor)
{
    m_auxiliaryColor = newAuxiliaryColor;
}

const QColor& PDF3DSceneProcessor::getFaceColor() const
{
    return m_faceColor;
}

void PDF3DSceneProcessor::setFaceColor(const QColor& newFaceColor)
{
    m_faceColor = newFaceColor;
}

const pdf::PDFReal& PDF3DSceneProcessor::getOpacity() const
{
    return m_opacity;
}

void PDF3DSceneProcessor::setOpacity(const pdf::PDFReal& newOpacity)
{
    m_opacity = newOpacity;
}

const pdf::PDFReal& PDF3DSceneProcessor::getCreaseAngle() const
{
    return m_creaseAngle;
}

void PDF3DSceneProcessor::setCreaseAngle(const pdf::PDFReal& newCreaseAngle)
{
    m_creaseAngle = newCreaseAngle;
}

const QString& PDF3DSceneProcessor::getSceneRoot() const
{
    return m_sceneRoot;
}

void PDF3DSceneProcessor::setSceneRoot(const QString& newSceneRoot)
{
    m_sceneRoot = newSceneRoot;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createNode(const pdf::u3d::PDF3D_U3D_Node& node)
{
    Qt3DCore::QNode* processedNode = nullptr;

    switch (node.getType())
    {
        case pdf::u3d::PDF3D_U3D_Node::Unknown:
            processedNode = new Qt3DCore::QNode(nullptr);
            break;

        case pdf::u3d::PDF3D_U3D_Node::Group:
        {
            if (node.hasChildTransform() && node.hasConstantChildTransform())
            {
                Qt3DCore::QEntity* entity = new Qt3DCore::QEntity(nullptr);
                Qt3DCore::QTransform* transform = new Qt3DCore::QTransform(nullptr);
                transform->setMatrix(node.getConstantChildTransform());
                entity->addComponent(transform);
                processedNode = entity;
            }
            else
            {
                processedNode = new Qt3DCore::QNode(nullptr);
            }
            break;
        }

        case pdf::u3d::PDF3D_U3D_Node::Model:
        {
            processedNode = createModelNode(node);
            break;
        }

        case pdf::u3d::PDF3D_U3D_Node::Light:
        {
            processedNode = createLightNode(node);
            break;
        }

        case pdf::u3d::PDF3D_U3D_Node::View:
            return nullptr;

        default:
            Q_ASSERT(false);
            break;
    }

    if (!processedNode)
    {
        return processedNode;
    }

    processedNode->setObjectName(node.getNodeName());

    for (const QString& childNodeName : node.getChildren())
    {
        Qt3DCore::QNode* childNode = createNode(m_sceneData->getNode(childNodeName));

        if (!childNode)
        {
            continue;
        }

        if (node.hasChildTransform() && !node.hasConstantChildTransform())
        {
            Qt3DCore::QEntity* entity = new Qt3DCore::QEntity(processedNode);
            Qt3DCore::QTransform* transform = new Qt3DCore::QTransform(nullptr);
            transform->setMatrix(node.getChildTransform(childNodeName));
            entity->addComponent(transform);
            childNode->setParent(entity);
        }
        else
        {
            childNode->setParent(processedNode);
        }
    }

    return processedNode;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createModelNode(const pdf::u3d::PDF3D_U3D_Node& node)
{
    Qt3DCore::QNode* processedNode = nullptr;

    if (const pdf::u3d::PDF3D_U3D_Geometry* geometry = m_sceneData->getGeometry(node.getResourceName()))
    {
        if (auto meshGeometry = geometry->asMeshGeometry())
        {
            processedNode = createMeshGeometry(meshGeometry);
        }
        else if (auto pointSetGeometry = geometry->asPointSetGeometry())
        {
            processedNode = createPointSetGeometry(pointSetGeometry);
        }
        else if (auto lineSetGeometry = geometry->asLineSetGeometry())
        {
            processedNode = createLineSetGeometry(lineSetGeometry);
        }
    }

    if (processedNode)
    {
        processedNode->setObjectName(node.getNodeName());
    }

    return processedNode;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createLightNode(const pdf::u3d::PDF3D_U3D_Node& node)
{
    Q_ASSERT(node.getType() == pdf::u3d::PDF3D_U3D_Node::NodeType::Light);

    if (const pdf::u3d::PDF3D_U3D_Light* light = m_sceneData->getLight(node.getResourceName()))
    {
        switch (light->getType())
        {
            case pdf::u3d::PDF3D_U3D_Light::Ambient:
            {
                m_globalAmbientColor.setRedF(m_globalAmbientColor.redF() + light->getColor().redF() * light->getIntensity());
                m_globalAmbientColor.setGreenF(m_globalAmbientColor.greenF() + light->getColor().greenF() * light->getIntensity());
                m_globalAmbientColor.setBlueF(m_globalAmbientColor.blueF() + light->getColor().blueF() * light->getIntensity());
                return nullptr;
            }

            case pdf::u3d::PDF3D_U3D_Light::Directional:
            {
                Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
                Qt3DRender::QDirectionalLight* processedLight = new Qt3DRender::QDirectionalLight();
                processedLight->setColor(light->getColor());
                processedLight->setIntensity(light->getIntensity());
                entity->addComponent(processedLight);
                return entity;
            }

            case pdf::u3d::PDF3D_U3D_Light::Point:
            {
                Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
                Qt3DRender::QPointLight* processedLight = new Qt3DRender::QPointLight();
                processedLight->setColor(light->getColor());
                processedLight->setIntensity(light->getIntensity());
                processedLight->setConstantAttenuation(light->getAttenuation()[0]);
                processedLight->setLinearAttenuation(light->getAttenuation()[1]);
                processedLight->setQuadraticAttenuation(light->getAttenuation()[2]);
                entity->addComponent(processedLight);
                return entity;
            }

            case pdf::u3d::PDF3D_U3D_Light::Spot:
            {
                Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
                Qt3DRender::QSpotLight* processedLight = new Qt3DRender::QSpotLight();
                processedLight->setColor(light->getColor());
                processedLight->setIntensity(light->getIntensity());
                processedLight->setConstantAttenuation(light->getAttenuation()[0]);
                processedLight->setLinearAttenuation(light->getAttenuation()[1]);
                processedLight->setQuadraticAttenuation(light->getAttenuation()[2]);
                processedLight->setCutOffAngle(light->getSpotAngle());
                entity->addComponent(processedLight);
                return entity;
            }

            default:
                Q_ASSERT(false);
                break;
        }
    }

    return nullptr;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createMeshGeometry(const pdf::u3d::PDF3D_U3D_MeshGeometry* meshGeometry)
{
    if (meshGeometry->isEmpty())
    {
        return nullptr;
    }

    switch (m_mode)
    {
        case BoundingBox:
        {
            // We will display bounding box only, bounding
            // box has only edges colored with auxiliary color and faces are missing.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(meshGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                return createBoundingBoxWireGeometry(boundingBox);
            }

            break;
        }

        case TransparentBoundingBox:
        {
            // We will display bounding box only, bounding
            // box has no edges missing and faces are transparent.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(meshGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                return createBoundingBoxTransparentGeometry(boundingBox);
            }

            break;
        }

        case TransparentBoundingBoxOutline:
        {
            // We will display bounding box only, bounding
            // box has edges colored with auxiliary color and faces are
            // transparent.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(meshGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                Qt3DCore::QNode* wireGeometry = createBoundingBoxWireGeometry(boundingBox);
                Qt3DCore::QNode* transparentGeometry = createBoundingBoxTransparentGeometry(boundingBox);

                Q_ASSERT(wireGeometry);
                Q_ASSERT(transparentGeometry);

                Qt3DCore::QNode* node = new Qt3DCore::QNode();
                wireGeometry->setParent(node);
                transparentGeometry->setParent(node);
                return node;
            }

            break;
        }

        case Wireframe:
        {
            // We will display lines colored by auxiliary color

            // Vertex buffer
            Qt3DRender::QAttribute* positionAttribute = createPositionAttribute(meshGeometry->getPositions());

            // Index buffer
            uint lineCount = static_cast<uint>(meshGeometry->getTriangleCount()) * 3;
            QByteArray indexBufferData;
            indexBufferData.resize(lineCount * 2 * sizeof(unsigned int));
            unsigned int* indexBufferDataPtr = reinterpret_cast<unsigned int*>(indexBufferData.data());

            Qt3DRender::QBuffer* indexBuffer = new Qt3DRender::QBuffer();
            indexBuffer->setType(Qt3DRender::QBuffer::IndexBuffer);

            for (const pdf::u3d::PDF3D_U3D_MeshGeometry::Triangle& triangle : meshGeometry->getTriangles())
            {
                *indexBufferDataPtr++ = triangle.vertices[0].positionIndex;
                *indexBufferDataPtr++ = triangle.vertices[1].positionIndex;
                *indexBufferDataPtr++ = triangle.vertices[1].positionIndex;
                *indexBufferDataPtr++ = triangle.vertices[2].positionIndex;
                *indexBufferDataPtr++ = triangle.vertices[2].positionIndex;
                *indexBufferDataPtr++ = triangle.vertices[0].positionIndex;
            }
            indexBuffer->setData(indexBufferData);

            Qt3DRender::QAttribute* indexAttribute = new Qt3DRender::QAttribute();
            indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
            indexAttribute->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
            indexAttribute->setBuffer(indexBuffer);
            indexAttribute->setCount(2 * lineCount);

            // Geometry
            Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
            geometry->addAttribute(positionAttribute);
            geometry->addAttribute(indexAttribute);

            Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
            geometryRenderer->setGeometry(geometry);
            geometryRenderer->setPrimitiveRestartEnabled(false);
            geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);

            Qt3DExtras::QDiffuseSpecularMaterial* material = new Qt3DExtras::QDiffuseSpecularMaterial();
            material->setAmbient(getAuxiliaryColor());
            material->setDiffuse(QColor(Qt::transparent));
            material->setSpecular(QColor(Qt::transparent));

            Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
            entity->addComponent(geometryRenderer);
            entity->addComponent(material);
            return entity;
        }

        case Vertices:
        {
            // We will display only vertices, with auxiliary color

            return createVertexGeometry(meshGeometry->getPositions());
        }

        case Solid:
        {
            // We will display solid color

            // Vertex buffer - we create vertex buffer with position(3), normal(3), color(4), texture coordinate (2)
            constexpr int positionVertexSize = 3;
            constexpr int normalVertexSize = 3;
            constexpr int colorVertexSize = 4;
            constexpr int textureVertexSize = 2;

            const uint triangleCount = static_cast<uint>(meshGeometry->getTriangleCount());
            constexpr uint32_t stride = (positionVertexSize + normalVertexSize + colorVertexSize + textureVertexSize) * sizeof(float);
            constexpr uint32_t positionVertexByteOffset = 0;
            constexpr uint32_t normalVertexByteOffset = positionVertexByteOffset + positionVertexSize * sizeof(float);
            constexpr uint32_t colorVertexByteOffset = normalVertexByteOffset + normalVertexSize * sizeof(float);
            constexpr uint32_t textureVertexByteOffset = colorVertexByteOffset + colorVertexSize * sizeof(float);

            QByteArray vertexBufferData;
            vertexBufferData.resize(triangleCount * 3 * stride);
            float* vertexBufferDataPtr = reinterpret_cast<float*>(vertexBufferData.data());

            bool hasTextures = false;
            bool hasDiffuse = false;
            bool hasSpecular = false;

            Qt3DRender::QBuffer* vertexBuffer = new Qt3DRender::QBuffer();
            vertexBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);

            for (const pdf::u3d::PDF3D_U3D_MeshGeometry::Triangle& triangle : meshGeometry->getTriangles())
            {
                hasTextures = hasTextures || triangle.hasTexture;
                hasDiffuse = hasDiffuse || triangle.hasDiffuse;
                hasSpecular = hasSpecular || triangle.hasSpecular;

                for (const pdf::u3d::PDF3D_U3D_MeshGeometry::Vertex& vertex : triangle.vertices)
                {
                    QVector3D position = meshGeometry->getPosition(vertex.positionIndex);
                    QVector3D normal = meshGeometry->getNormal(vertex.normalIndex);
                    QVector4D diffuseColor = meshGeometry->getDiffuseColor(vertex.diffuseColorIndex);
                    QVector4D textureCoordinate = meshGeometry->getTextureCoordinate(vertex.textureCoordIndex);

                    // Vertex
                    *vertexBufferDataPtr++ = position[0];
                    *vertexBufferDataPtr++ = position[1];
                    *vertexBufferDataPtr++ = position[2];

                    // Normal
                    *vertexBufferDataPtr++ = normal[0];
                    *vertexBufferDataPtr++ = normal[1];
                    *vertexBufferDataPtr++ = normal[2];

                    // Diffuse
                    *vertexBufferDataPtr++ = diffuseColor[0];
                    *vertexBufferDataPtr++ = diffuseColor[1];
                    *vertexBufferDataPtr++ = diffuseColor[2];
                    *vertexBufferDataPtr++ = diffuseColor[3];

                    // Texture coordinate
                    *vertexBufferDataPtr++ = textureCoordinate[0];
                    *vertexBufferDataPtr++ = textureCoordinate[1];

                    Q_ASSERT(vertexBufferDataPtr <= reinterpret_cast<float*>(vertexBufferData.data() + vertexBufferData.size()));
                }
            }
            vertexBuffer->setData(vertexBufferData);

            // Position attribute
            Qt3DRender::QAttribute* positionAttribute = new Qt3DRender::QAttribute();
            positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
            positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
            positionAttribute->setDataType(Qt3DRender::QAttribute::Float);
            positionAttribute->setDataSize(positionVertexSize);
            positionAttribute->setBuffer(vertexBuffer);
            positionAttribute->setByteOffset(positionVertexByteOffset);
            positionAttribute->setByteStride(stride);
            positionAttribute->setCount(triangleCount * 3);

            // Normal attribute
            Qt3DRender::QAttribute* normalAttribute = new Qt3DRender::QAttribute();
            normalAttribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());
            normalAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
            normalAttribute->setDataType(Qt3DRender::QAttribute::Float);
            normalAttribute->setDataSize(normalVertexSize);
            normalAttribute->setBuffer(vertexBuffer);
            normalAttribute->setByteOffset(normalVertexByteOffset);
            normalAttribute->setByteStride(stride);
            normalAttribute->setCount(triangleCount * 3);

            // Color attribute
            Qt3DRender::QAttribute* colorAttribute = new Qt3DRender::QAttribute();
            colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());
            colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
            colorAttribute->setDataType(Qt3DRender::QAttribute::Float);
            colorAttribute->setDataSize(colorVertexSize);
            colorAttribute->setBuffer(vertexBuffer);
            colorAttribute->setByteOffset(colorVertexByteOffset);
            colorAttribute->setByteStride(stride);
            colorAttribute->setCount(triangleCount * 3);

            // Texture attribute
            Qt3DRender::QAttribute* textureAttribute = new Qt3DRender::QAttribute();
            textureAttribute->setName(Qt3DRender::QAttribute::defaultTextureCoordinateAttributeName());
            textureAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
            textureAttribute->setDataType(Qt3DRender::QAttribute::Float);
            textureAttribute->setDataSize(textureVertexSize);
            textureAttribute->setBuffer(vertexBuffer);
            textureAttribute->setByteOffset(textureVertexByteOffset);
            textureAttribute->setByteStride(stride);
            textureAttribute->setCount(triangleCount * 3);

            // Geometry
            Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
            geometry->addAttribute(positionAttribute);
            geometry->addAttribute(normalAttribute);
            geometry->addAttribute(colorAttribute);
            geometry->addAttribute(textureAttribute);

            Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
            geometryRenderer->setGeometry(geometry);
            geometryRenderer->setPrimitiveRestartEnabled(false);
            geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

            Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
            entity->addComponent(geometryRenderer);

            QString shaderName = meshGeometry->getShaderName(0);
            Qt3DRender::QMaterial* material = createMaterialFromShader(shaderName, false);
            entity->addComponent(material);

            return entity;
        }

        case Illustration:
        case ShadedIllustration:
        case ShadedWireframe:
        case HiddenWireframe:
        case SolidOutline:
        case Transparent:
        case TransparentWireframe:
        case SolidWireframe:
        case ShadedVertices:

        default:
            Q_ASSERT(false);
            break;
    }

    return nullptr;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createPointSetGeometry(const pdf::u3d::PDF3D_U3D_PointSetGeometry* pointSetGeometry)
{
    if (pointSetGeometry->isEmpty())
    {
        return nullptr;
    }

    switch (m_mode)
    {
        case BoundingBox:
        {
            // We will display bounding box only, bounding
            // box has only edges colored with auxiliary color and faces are missing.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(pointSetGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                return createBoundingBoxWireGeometry(boundingBox);
            }

            break;
        }

        case TransparentBoundingBox:
        {
            // We will display bounding box only, bounding
            // box has no edges missing and faces are transparent.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(pointSetGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                return createBoundingBoxTransparentGeometry(boundingBox);
            }

            break;
        }

        case TransparentBoundingBoxOutline:
        {
            // We will display bounding box only, bounding
            // box has edges colored with auxiliary color and faces are
            // transparent.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(pointSetGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                Qt3DCore::QNode* wireGeometry = createBoundingBoxWireGeometry(boundingBox);
                Qt3DCore::QNode* transparentGeometry = createBoundingBoxTransparentGeometry(boundingBox);

                Q_ASSERT(wireGeometry);
                Q_ASSERT(transparentGeometry);

                Qt3DCore::QNode* node = new Qt3DCore::QNode();
                wireGeometry->setParent(node);
                transparentGeometry->setParent(node);
                return node;
            }

            break;
        }

        case Illustration:
        case ShadedIllustration:
        case Wireframe:
        case Vertices:
        {
            // We will display only vertices, with auxiliary color

            return createVertexGeometry(pointSetGeometry->getPositions());
        }

        case ShadedWireframe:
        case HiddenWireframe:
        case SolidOutline:
        case Transparent:
        case TransparentWireframe:
        case Solid:
        case SolidWireframe:
        case ShadedVertices:
        {
            // We will display vertices with line color

            // Vertex buffer
            Qt3DRender::QAttribute* positionAttribute = createPositionAttribute(pointSetGeometry->getPositions());

            // Color buffer
            Qt3DRender::QBuffer* colorBuffer = new Qt3DRender::QBuffer();
            colorBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);
            const uint positionCount = positionAttribute->count();

            QByteArray colorBufferData;
            colorBufferData.resize(positionCount * 3 * sizeof(float));
            float* colorBufferDataPtr = reinterpret_cast<float*>(colorBufferData.data());

            for (size_t i = 0; i < positionCount; ++i)
            {
                QVector3D color(0.0, 0.0, 0.0);
                std::vector<pdf::u3d::PDF3D_U3D_PointSetGeometry::Point> points = pointSetGeometry->queryPointsByVertexIndex(i);

                if (!points.empty())
                {
                    pdf::u3d::PDF3D_U3D_PointSetGeometry::Point point = points.front();
                    color = pointSetGeometry->getDiffuseColor(point.diffuseColor).toVector3D();
                }

                *colorBufferDataPtr++ = color.x();
                *colorBufferDataPtr++ = color.y();
                *colorBufferDataPtr++ = color.z();
            }
            colorBuffer->setData(colorBufferData);

            Qt3DRender::QAttribute* colorAttribute = new Qt3DRender::QAttribute();
            colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());
            colorAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
            colorAttribute->setVertexSize(3);
            colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
            colorAttribute->setBuffer(colorBuffer);
            colorAttribute->setByteOffset(0);
            colorAttribute->setByteStride(3 * sizeof(float));
            colorAttribute->setCount(positionCount);

            // Geometry
            Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
            geometry->addAttribute(positionAttribute);
            geometry->addAttribute(colorAttribute);

            Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
            geometryRenderer->setGeometry(geometry);
            geometryRenderer->setPrimitiveRestartEnabled(false);
            geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Points);

            Qt3DExtras::QPerVertexColorMaterial* material = new Qt3DExtras::QPerVertexColorMaterial();

            Qt3DRender::QEffect* effect = material->effect();
            for (Qt3DRender::QTechnique* technique : effect->techniques())
            {
                for (Qt3DRender::QRenderPass* renderPass : technique->renderPasses())
                {
                    Qt3DRender::QPointSize* pointSize = new Qt3DRender::QPointSize();
                    pointSize->setSizeMode(Qt3DRender::QPointSize::Fixed);
                    pointSize->setValue(m_pointSize);
                    renderPass->addRenderState(pointSize);
                }
            }

            Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
            entity->addComponent(geometryRenderer);
            entity->addComponent(material);
            return entity;
        }

        default:
            Q_ASSERT(false);
            break;
    }

    return nullptr;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createLineSetGeometry(const pdf::u3d::PDF3D_U3D_LineSetGeometry* lineSetGeometry)
{
    if (lineSetGeometry->isEmpty())
    {
        return nullptr;
    }

    switch (m_mode)
    {
        case BoundingBox:
        {
            // We will display bounding box only, bounding
            // box has only edges colored with auxiliary color and faces are missing.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(lineSetGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                return createBoundingBoxWireGeometry(boundingBox);
            }

            break;
        }

        case TransparentBoundingBox:
        {
            // We will display bounding box only, bounding
            // box has no edges missing and faces are transparent.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(lineSetGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                return createBoundingBoxTransparentGeometry(boundingBox);
            }

            break;
        }

        case TransparentBoundingBoxOutline:
        {
            // We will display bounding box only, bounding
            // box has edges colored with auxiliary color and faces are
            // transparent.

            PDF3DBoundingBox boundingBox = PDF3DBoundingBox::getBoundingBox(lineSetGeometry->getPositions());
            if (!boundingBox.isEmpty())
            {
                Qt3DCore::QNode* wireGeometry = createBoundingBoxWireGeometry(boundingBox);
                Qt3DCore::QNode* transparentGeometry = createBoundingBoxTransparentGeometry(boundingBox);

                Q_ASSERT(wireGeometry);
                Q_ASSERT(transparentGeometry);

                Qt3DCore::QNode* node = new Qt3DCore::QNode();
                wireGeometry->setParent(node);
                transparentGeometry->setParent(node);
                return node;
            }

            break;
        }

        case Illustration:
        case ShadedIllustration:
        case Wireframe:
        {
            // We will display lines colored by auxiliary color

            // Vertex buffer
            Qt3DRender::QAttribute* positionAttribute = createPositionAttribute(lineSetGeometry->getPositions());

            // Index buffer
            uint lineCount = static_cast<uint>(lineSetGeometry->getLineCount());
            QByteArray indexBufferData;
            indexBufferData.resize(lineCount * 2 * sizeof(unsigned int));
            unsigned int* indexBufferDataPtr = reinterpret_cast<unsigned int*>(indexBufferData.data());

            Qt3DRender::QBuffer* indexBuffer = new Qt3DRender::QBuffer();
            indexBuffer->setType(Qt3DRender::QBuffer::IndexBuffer);

            for (size_t i = 0; i < lineCount; ++i)
            {
                const auto& line = lineSetGeometry->getLine(i);
                *indexBufferDataPtr++ = line.position1;
                *indexBufferDataPtr++ = line.position2;
            }
            indexBuffer->setData(indexBufferData);

            Qt3DRender::QAttribute* indexAttribute = new Qt3DRender::QAttribute();
            indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
            indexAttribute->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
            indexAttribute->setBuffer(indexBuffer);
            indexAttribute->setCount(2 * lineCount);

            // Geometry
            Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
            geometry->addAttribute(positionAttribute);
            geometry->addAttribute(indexAttribute);

            Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
            geometryRenderer->setGeometry(geometry);
            geometryRenderer->setPrimitiveRestartEnabled(false);
            geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);

            Qt3DExtras::QDiffuseSpecularMaterial* material = new Qt3DExtras::QDiffuseSpecularMaterial();
            material->setAmbient(getAuxiliaryColor());
            material->setDiffuse(QColor(Qt::transparent));
            material->setSpecular(QColor(Qt::transparent));

            Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
            entity->addComponent(geometryRenderer);
            entity->addComponent(material);
            return entity;
        }

        case ShadedWireframe:
        case HiddenWireframe:
        case SolidOutline:
        case Transparent:
        case TransparentWireframe:
        case Solid:
        case SolidWireframe:
        {
            // We will display classic colored lines

            using Line = pdf::u3d::PDF3D_U3D_LineSetGeometry::Line;
            std::map<uint32_t, std::vector<Line>> shadingIdToLines;

            for (const Line& line : lineSetGeometry->getLines())
            {
                shadingIdToLines[line.shadingId].push_back(line);
            }

            std::vector<Qt3DCore::QEntity*> entities;

            for (const auto& item : shadingIdToLines)
            {
                const uint32_t shadingId = item.first;
                const std::vector<Line>& lines = item.second;

                std::vector<QVector3D> positions;
                std::vector<QVector3D> colors;

                positions.reserve(lines.size() * 2);
                colors.reserve(lines.size() * 2);

                for (const Line& line : lines)
                {
                    positions.push_back(lineSetGeometry->getPosition(line.position1));
                    positions.push_back(lineSetGeometry->getPosition(line.position2));
                    colors.push_back(lineSetGeometry->getDiffuseColor(line.diffuseColor1).toVector3D());
                    colors.push_back(lineSetGeometry->getDiffuseColor(line.diffuseColor2).toVector3D());
                }

                Qt3DRender::QAttribute* positionAttribute = createPositionAttribute(positions);
                Qt3DRender::QAttribute* colorAttribute = createColorAttribute(colors);

                // Geometry
                Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
                geometry->addAttribute(positionAttribute);
                geometry->addAttribute(colorAttribute);

                Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
                geometryRenderer->setGeometry(geometry);
                geometryRenderer->setPrimitiveRestartEnabled(false);
                geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);

                QString shaderName = lineSetGeometry->getShaderName(shadingId);
                Qt3DRender::QMaterial* material = createMaterialFromShader(shaderName, true);

                Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
                entity->addComponent(geometryRenderer);
                entity->addComponent(material);
                entities.emplace_back(entity);
            }

            if (entities.size() == 1)
            {
                return entities.front();
            }
            else
            {
                Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();

                for (Qt3DCore::QEntity* currentEntity : entities)
                {
                    currentEntity->setParent(entity);
                }

                return entity;
            }
        }

        case Vertices:
        {
            // We will display only vertices, with auxiliary color

            return createVertexGeometry(lineSetGeometry->getPositions());
        }

        case ShadedVertices:
        {
            // We will display vertices with line color

            // Vertex buffer
            Qt3DRender::QAttribute* positionAttribute = createPositionAttribute(lineSetGeometry->getPositions());

            // Color buffer
            Qt3DRender::QBuffer* colorBuffer = new Qt3DRender::QBuffer();
            colorBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);
            const uint positionCount = positionAttribute->count();

            QByteArray colorBufferData;
            colorBufferData.resize(positionCount * 3 * sizeof(float));
            float* colorBufferDataPtr = reinterpret_cast<float*>(colorBufferData.data());

            for (size_t i = 0; i < positionCount; ++i)
            {
                QVector3D color(0.0, 0.0, 0.0);
                std::vector<pdf::u3d::PDF3D_U3D_LineSetGeometry::Line> lines = lineSetGeometry->queryLinesByVertexIndex(i);

                if (!lines.empty())
                {
                    pdf::u3d::PDF3D_U3D_LineSetGeometry::Line line = lines.front();

                    if (line.position1 == i)
                    {
                        color = lineSetGeometry->getDiffuseColor(line.diffuseColor1).toVector3D();
                    }

                    if (line.position2 == i)
                    {
                        color = lineSetGeometry->getDiffuseColor(line.diffuseColor2).toVector3D();
                    }
                }

                *colorBufferDataPtr++ = color.x();
                *colorBufferDataPtr++ = color.y();
                *colorBufferDataPtr++ = color.z();
            }
            colorBuffer->setData(colorBufferData);

            Qt3DRender::QAttribute* colorAttribute = new Qt3DRender::QAttribute();
            colorAttribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());
            colorAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
            colorAttribute->setVertexSize(3);
            colorAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
            colorAttribute->setBuffer(colorBuffer);
            colorAttribute->setByteOffset(0);
            colorAttribute->setByteStride(3 * sizeof(float));
            colorAttribute->setCount(positionCount);

            // Geometry
            Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
            geometry->addAttribute(positionAttribute);
            geometry->addAttribute(colorAttribute);

            Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
            geometryRenderer->setGeometry(geometry);
            geometryRenderer->setPrimitiveRestartEnabled(false);
            geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Points);

            Qt3DExtras::QPerVertexColorMaterial* material = new Qt3DExtras::QPerVertexColorMaterial();

            Qt3DRender::QEffect* effect = material->effect();
            for (Qt3DRender::QTechnique* technique : effect->techniques())
            {
                for (Qt3DRender::QRenderPass* renderPass : technique->renderPasses())
                {
                    Qt3DRender::QPointSize* pointSize = new Qt3DRender::QPointSize();
                    pointSize->setSizeMode(Qt3DRender::QPointSize::Fixed);
                    pointSize->setValue(m_pointSize);
                    renderPass->addRenderState(pointSize);
                }
            }

            Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
            entity->addComponent(geometryRenderer);
            entity->addComponent(material);
            return entity;
        }

        default:
            Q_ASSERT(false);
            break;
    }

    return nullptr;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createVertexGeometry(const std::vector<QVector3D>& positions)
{
    // Vertex buffer
    Qt3DRender::QAttribute* positionAttribute = createPositionAttribute(positions);

    // Geometry
    Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
    geometry->addAttribute(positionAttribute);

    Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
    geometryRenderer->setGeometry(geometry);
    geometryRenderer->setPrimitiveRestartEnabled(false);
    geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Points);

    Qt3DExtras::QDiffuseSpecularMaterial* material = new Qt3DExtras::QDiffuseSpecularMaterial();
    material->setAmbient(getAuxiliaryColor());
    material->setDiffuse(QColor(Qt::transparent));
    material->setSpecular(QColor(Qt::transparent));

    Qt3DRender::QEffect* effect = material->effect();
    for (Qt3DRender::QTechnique* technique : effect->techniques())
    {
        for (Qt3DRender::QRenderPass* renderPass : technique->renderPasses())
        {
            Qt3DRender::QPointSize* pointSize = new Qt3DRender::QPointSize();
            pointSize->setSizeMode(Qt3DRender::QPointSize::Fixed);
            pointSize->setValue(m_pointSize);
            renderPass->addRenderState(pointSize);
        }
    }

    Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
    entity->addComponent(geometryRenderer);
    entity->addComponent(material);
    return entity;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createBoundingBoxWireGeometry(const PDF3DBoundingBox& boundingBox)
{
    std::array<QVector3D, 8> positions;

    QVector3D min = boundingBox.getMin();
    QVector3D max = boundingBox.getMax();

    positions[0] = QVector3D(min.x(), min.y(), min.z());
    positions[1] = QVector3D(max.x(), min.y(), min.z());
    positions[2] = QVector3D(max.x(), max.y(), min.z());
    positions[3] = QVector3D(min.x(), max.y(), min.z());
    positions[4] = QVector3D(min.x(), min.y(), max.z());
    positions[5] = QVector3D(max.x(), min.y(), max.z());
    positions[6] = QVector3D(max.x(), max.y(), max.z());
    positions[7] = QVector3D(min.x(), max.y(), max.z());

    std::array<uint, 24> indices = {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        4, 5,
        5, 6,
        6, 7,
        7, 4,
        0, 4,
        1, 5,
        2, 6,
        3, 7
    };

    // Vertex buffer
    Qt3DRender::QBuffer* vertexBuffer = new Qt3DRender::QBuffer();
    vertexBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);

    uint positionCount = static_cast<uint>(positions.size());
    QByteArray vertexBufferData;
    vertexBufferData.resize(positionCount * 3 * sizeof(float));
    float* vertexBufferDataPtr = reinterpret_cast<float*>(vertexBufferData.data());

    for (size_t i = 0; i < positionCount; ++i)
    {
        QVector3D position = positions[i];
        *vertexBufferDataPtr++ = position.x();
        *vertexBufferDataPtr++ = position.y();
        *vertexBufferDataPtr++ = position.z();
    }
    vertexBuffer->setData(vertexBufferData);

    Qt3DRender::QAttribute* positionAttribute = new Qt3DRender::QAttribute();
    positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(vertexBuffer);
    positionAttribute->setByteOffset(0);
    positionAttribute->setByteStride(3 * sizeof(float));
    positionAttribute->setCount(positionCount);

    // Index buffer
    uint lineCount = static_cast<uint>(indices.size()) / 2;
    QByteArray indexBufferData;
    indexBufferData.resize(lineCount * 2 * sizeof(unsigned int));
    unsigned int* indexBufferDataPtr = reinterpret_cast<unsigned int*>(indexBufferData.data());

    Qt3DRender::QBuffer* indexBuffer = new Qt3DRender::QBuffer();
    indexBuffer->setType(Qt3DRender::QBuffer::IndexBuffer);

    for (size_t i = 0; i < indices.size(); ++i)
    {
        *indexBufferDataPtr++ = indices[i];
    }
    indexBuffer->setData(indexBufferData);

    Qt3DRender::QAttribute* indexAttribute = new Qt3DRender::QAttribute();
    indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    indexAttribute->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    indexAttribute->setBuffer(indexBuffer);
    indexAttribute->setCount(2 * lineCount);

    // Geometry
    Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
    geometry->addAttribute(positionAttribute);
    geometry->addAttribute(indexAttribute);

    Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
    geometryRenderer->setGeometry(geometry);
    geometryRenderer->setPrimitiveRestartEnabled(false);
    geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Lines);

    Qt3DExtras::QDiffuseSpecularMaterial* material = new Qt3DExtras::QDiffuseSpecularMaterial();
    material->setAmbient(getAuxiliaryColor());
    material->setDiffuse(QColor(Qt::transparent));
    material->setSpecular(QColor(Qt::transparent));

    Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
    entity->addComponent(geometryRenderer);
    entity->addComponent(material);
    return entity;
}

Qt3DCore::QNode* PDF3DSceneProcessor::createBoundingBoxTransparentGeometry(const PDF3DBoundingBox& boundingBox)
{
    std::array<QVector3D, 8> positions;

    QVector3D min = boundingBox.getMin();
    QVector3D max = boundingBox.getMax();

    positions[0] = QVector3D(min.x(), min.y(), min.z());
    positions[1] = QVector3D(max.x(), min.y(), min.z());
    positions[2] = QVector3D(max.x(), max.y(), min.z());
    positions[3] = QVector3D(min.x(), max.y(), min.z());
    positions[4] = QVector3D(min.x(), min.y(), max.z());
    positions[5] = QVector3D(max.x(), min.y(), max.z());
    positions[6] = QVector3D(max.x(), max.y(), max.z());
    positions[7] = QVector3D(min.x(), max.y(), max.z());

    std::array<uint, 36> indices = {
        0, 1, 2, // Bottom
        2, 3, 0,
        4, 5, 6, // Top
        6, 7, 4,
        0, 1, 5, // Side 1
        0, 4, 5,
        1, 2, 6, // Side 2
        1, 5, 6,
        2, 3, 7, // Side 3
        2, 6, 7,
        3, 0, 4, // Side 4
        3, 7, 4
    };

    // Vertex buffer
    Qt3DRender::QBuffer* vertexBuffer = new Qt3DRender::QBuffer();
    vertexBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);

    uint positionCount = static_cast<uint>(positions.size());
    QByteArray vertexBufferData;
    vertexBufferData.resize(positionCount * 3 * sizeof(float));
    float* vertexBufferDataPtr = reinterpret_cast<float*>(vertexBufferData.data());

    for (size_t i = 0; i < positionCount; ++i)
    {
        QVector3D position = positions[i];
        *vertexBufferDataPtr++ = position.x();
        *vertexBufferDataPtr++ = position.y();
        *vertexBufferDataPtr++ = position.z();
    }
    vertexBuffer->setData(vertexBufferData);

    Qt3DRender::QAttribute* positionAttribute = new Qt3DRender::QAttribute();
    positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(vertexBuffer);
    positionAttribute->setByteOffset(0);
    positionAttribute->setByteStride(3 * sizeof(float));
    positionAttribute->setCount(positionCount);

    // Index buffer
    uint triangleCount = static_cast<uint>(indices.size()) / 3;
    QByteArray indexBufferData;
    indexBufferData.resize(triangleCount * 3 * sizeof(unsigned int));
    unsigned int* indexBufferDataPtr = reinterpret_cast<unsigned int*>(indexBufferData.data());

    Qt3DRender::QBuffer* indexBuffer = new Qt3DRender::QBuffer();
    indexBuffer->setType(Qt3DRender::QBuffer::IndexBuffer);

    for (size_t i = 0; i < indices.size(); ++i)
    {
        *indexBufferDataPtr++ = indices[i];
    }
    indexBuffer->setData(indexBufferData);

    Qt3DRender::QAttribute* indexAttribute = new Qt3DRender::QAttribute();
    indexAttribute->setAttributeType(Qt3DRender::QAttribute::IndexAttribute);
    indexAttribute->setVertexBaseType(Qt3DRender::QAttribute::UnsignedInt);
    indexAttribute->setBuffer(indexBuffer);
    indexAttribute->setCount(3 * triangleCount);

    // Geometry
    Qt3DRender::QGeometry* geometry = new Qt3DRender::QGeometry();
    geometry->addAttribute(positionAttribute);
    geometry->addAttribute(indexAttribute);

    Qt3DRender::QGeometryRenderer* geometryRenderer = new Qt3DRender::QGeometryRenderer();
    geometryRenderer->setGeometry(geometry);
    geometryRenderer->setPrimitiveRestartEnabled(false);
    geometryRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);

    QColor color = getAuxiliaryColor();
    color.setAlphaF(getOpacity());

    Qt3DExtras::QDiffuseSpecularMaterial* material = new Qt3DExtras::QDiffuseSpecularMaterial();
    material->setAmbient(QColor(Qt::transparent));
    material->setDiffuse(color);
    material->setSpecular(QColor(Qt::transparent));
    material->setAlphaBlendingEnabled(true);

    Qt3DRender::QEffect* effect = material->effect();
    for (Qt3DRender::QTechnique* technique : effect->techniques())
    {
        for (Qt3DRender::QRenderPass* renderPass : technique->renderPasses())
        {
            Qt3DRender::QCullFace* cullFace = new Qt3DRender::QCullFace();
            cullFace->setMode(Qt3DRender::QCullFace::NoCulling);
            renderPass->addRenderState(cullFace);
        }
    }

    Qt3DCore::QEntity* entity = new Qt3DCore::QEntity();
    entity->addComponent(geometryRenderer);
    entity->addComponent(material);
    return entity;
}

Qt3DRender::QAttribute* PDF3DSceneProcessor::createGenericAttribute(const std::vector<QVector3D>& values) const
{
    Qt3DRender::QBuffer* vertexBuffer = new Qt3DRender::QBuffer();
    vertexBuffer->setType(Qt3DRender::QBuffer::VertexBuffer);

    uint positionCount = static_cast<uint>(values.size());
    QByteArray vertexBufferData;
    vertexBufferData.resize(positionCount * 3 * sizeof(float));
    float* vertexBufferDataPtr = reinterpret_cast<float*>(vertexBufferData.data());

    for (size_t i = 0; i < positionCount; ++i)
    {
        QVector3D position = values[i];
        *vertexBufferDataPtr++ = position.x();
        *vertexBufferDataPtr++ = position.y();
        *vertexBufferDataPtr++ = position.z();
    }
    vertexBuffer->setData(vertexBufferData);

    Qt3DRender::QAttribute* positionAttribute = new Qt3DRender::QAttribute();
    positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(vertexBuffer);
    positionAttribute->setByteOffset(0);
    positionAttribute->setByteStride(3 * sizeof(float));
    positionAttribute->setCount(positionCount);

    return positionAttribute;
}

Qt3DRender::QAttribute* PDF3DSceneProcessor::createPositionAttribute(const std::vector<QVector3D>& positions) const
{
    Qt3DRender::QAttribute* attribute = createGenericAttribute(positions);
    attribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    return attribute;
}

Qt3DRender::QAttribute* PDF3DSceneProcessor::createNormalAttribute(const std::vector<QVector3D>& normals) const
{
    Qt3DRender::QAttribute* attribute = createGenericAttribute(normals);
    attribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());
    return attribute;
}

Qt3DRender::QAttribute* PDF3DSceneProcessor::createColorAttribute(const std::vector<QVector3D>& colors) const
{
    Qt3DRender::QAttribute* attribute = createGenericAttribute(colors);
    attribute->setName(Qt3DRender::QAttribute::defaultColorAttributeName());
    return attribute;
}

Qt3DRender::QMaterial* PDF3DSceneProcessor::createMaterialFromShader(const QString& shaderName, bool forceUseVertexColors) const
{
    Qt3DRender::QMaterial* material = nullptr;
    const pdf::u3d::PDF3D_U3D_Shader* shader = m_sceneData->getShader(shaderName);
    const pdf::u3d::PDF3D_U3D_Shader::PDF_U3D_TextureInfos& textures = shader->getTextureInfos();

    if (textures.empty())
    {
        if (shader->isUseVertexColor() || forceUseVertexColors)
        {
            material = new Qt3DExtras::QPerVertexColorMaterial();
        }
        else
        {
            const pdf::u3d::PDF3D_U3D_Material* u3dMaterial = m_sceneData->getMaterial(shader->getMaterialName());
            Qt3DExtras::QDiffuseSpecularMaterial* currentMaterial = new Qt3DExtras::QDiffuseSpecularMaterial();
            currentMaterial->setAmbient(u3dMaterial->getAmbientColor());
            currentMaterial->setDiffuse(u3dMaterial->getDiffuseColor());
            currentMaterial->setSpecular(u3dMaterial->getSpecularColor());
            currentMaterial->setAlphaBlendingEnabled(shader->isAlphaTestEnabled());
            material = currentMaterial;
        }
    }
    else
    {
        // Use texture material

        Qt3DExtras::QDiffuseSpecularMaterial* textureMaterial = new Qt3DExtras::QDiffuseSpecularMaterial();
        textureMaterial->setAlphaBlendingEnabled(shader->isAlphaTestEnabled());
        const pdf::u3d::PDF3D_U3D_Shader::PDF_U3D_TextureInfo& textureInfo = shader->getTextureInfos().front();
        QString textureName = textureInfo.textureName;
        QImage image = m_sceneData->getTexture(textureName);

        PDF3DTextureImage* textureImage = new PDF3DTextureImage(image, textureMaterial);
        textureImage->setSize(image.size());
        textureImage->update();

        Qt3DRender::QTexture2D* texture = new Qt3DRender::QTexture2D(textureMaterial);
        texture->setSize(image.width(), image.height());
        texture->addTextureImage(textureImage);

        textureMaterial->setAmbient(Qt::transparent);
        textureMaterial->setDiffuse(QVariant::fromValue<Qt3DRender::QAbstractTexture*>(texture));
        textureMaterial->setSpecular(QColor(Qt::transparent));

        material = textureMaterial;
    }

    if (shader->isAlphaTestEnabled())
    {
        Qt3DRender::QEffect* effect = material->effect();
        Qt3DRender::QAlphaTest* alphaTest = new Qt3DRender::QAlphaTest(material);
        alphaTest->setReferenceValue(shader->getAlphaTestReference());

        switch (shader->getAlphaTestFunction())
        {
            case pdf::u3d::PDF3D_U3D_Shader::NEVER:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::Never);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::LESS:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::Less);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::GREATER:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::Greater);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::EQUAL:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::Equal);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::NOT_EQUAL:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::NotEqual);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::LEQUAL:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::LessOrEqual);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::GEQUAL:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::GreaterOrEqual);
                break;
            case pdf::u3d::PDF3D_U3D_Shader::ALWAYS:
                alphaTest->setAlphaFunction(Qt3DRender::QAlphaTest::Always);
                break;

            default:
                Q_ASSERT(false);
                break;
        }

        for (Qt3DRender::QTechnique* technique : effect->techniques())
        {
            for (Qt3DRender::QRenderPass* renderPass : technique->renderPasses())
            {
                renderPass->addRenderState(alphaTest);
            }
        }
    }

    return material;
}

pdf::PDFReal PDF3DSceneProcessor::getPointSize() const
{
    return m_pointSize;
}

void PDF3DSceneProcessor::setPointSize(pdf::PDFReal newPointSize)
{
    m_pointSize = newPointSize;
}

PDF3DBoundingBox PDF3DBoundingBox::getBoundingBox(const std::vector<QVector3D>& points)
{
    if (points.empty())
    {
        return PDF3DBoundingBox();
    }

    QVector3D min = points.front();
    QVector3D max = points.front();

    for (const QVector3D& point : points)
    {
        min.setX(qMin(min.x(), point.x()));
        min.setY(qMin(min.y(), point.y()));
        min.setZ(qMin(min.z(), point.z()));

        max.setX(qMax(max.x(), point.x()));
        max.setY(qMax(max.y(), point.y()));
        max.setZ(qMax(max.z(), point.z()));
    }

    return PDF3DBoundingBox(min, max);
}

PDF3DBoundingBox::PDF3DBoundingBox(QVector3D min, QVector3D max) :
    m_min(min),
    m_max(max)
{

}

} // namespace pdfviewer