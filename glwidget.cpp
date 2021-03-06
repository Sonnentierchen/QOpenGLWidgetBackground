/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "glwidget.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMouseEvent>

#define PROGRAM_VERTEX_ATTRIBUTE 0
#define PROGRAM_TEXCOORD_ATTRIBUTE 1

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      clearColor(Qt::black),
      xRot(0),
      yRot(0),
      zRot(0),
      backgroundProgram(0),
      m_objectModelNormalVbo(QOpenGLBuffer::VertexBuffer),
      m_objectModelVertexVbo(QOpenGLBuffer::VertexBuffer),
      m_objectModelIndexVbo(QOpenGLBuffer::IndexBuffer)
{
}

GLWidget::~GLWidget()
{
    makeCurrent();
    backgroundVbo.destroy();
    delete backgroundTexture;
    delete backgroundProgram;
    doneCurrent();
}

static void qNormalizeAngle(int &angle)
{
    while (angle < 0)
        angle += 360 * 16;
    while (angle > 360 * 16)
        angle -= 360 * 16;
}

void GLWidget::setXRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != xRot) {
        xRot = angle;
        update();
    }
}

void GLWidget::setYRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != yRot) {
        yRot = angle;
        update();
    }
}

void GLWidget::setZRotation(int angle)
{
    qNormalizeAngle(angle);
    if (angle != zRot) {
        zRot = angle;
        update();
    }
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(50, 50);
}

QSize GLWidget::sizeHint() const
{
    return QSize(200, 200);
}

void GLWidget::rotateBy(int xAngle, int yAngle, int zAngle)
{
    xRot += xAngle;
    yRot += yAngle;
    zRot += zAngle;
    update();
}

void GLWidget::setClearColor(const QColor &color)
{
    clearColor = color;
    update();
}

const char *vertexShaderBackgroundSource =
        "attribute highp vec4 vertex;\n"
        "attribute mediump vec4 texCoord;\n"
        "varying mediump vec4 texc;\n"
        "uniform mediump mat4 matrix;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = matrix * vertex;\n"
        "    texc = texCoord;\n"
        "}\n";

const char *fragmentShaderBackgroundSource =
        "uniform sampler2D texture;\n"
        "varying mediump vec4 texc;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = texture2D(texture, texc.st);\n"
        "}\n";

static const char *vertexShaderObjectSource =
        "attribute vec4 vertex;\n"
        "attribute vec3 normal;\n"
        "varying vec3 vert;\n"
        "varying vec3 vertNormal;\n"
        "uniform mat4 projectionMatrix;\n"
        "uniform mat3 normalMatrix;\n"
        "void main() {\n"
        "   vert = vertex.xyz;\n"
        "   vertNormal = normalMatrix * normal;\n"
        "   gl_Position = projectionMatrix * vertex;\n"
        "}\n";

static const char *fragmentShaderObjectSource =
        "varying highp vec3 vert;\n"
        "varying highp vec3 vertNormal;\n"
        "uniform highp vec3 lightPos;\n"
        "void main() {\n"
        "   highp vec3 L = normalize(lightPos - vert);\n"
        "   highp float NL = max(dot(normalize(vertNormal), L), 0.0);\n"
        "   highp vec3 color = vec3(0.39, 1.0, 0.0);\n"
        "   highp vec3 col = clamp(color * 0.2 + color * 0.8 * NL, 0.0, 1.0);\n"
        "   gl_FragColor = vec4(col, 0.5);\n"
        "}\n";

void GLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    makeBackgroundObject();
    initializeBackgroundProgram();
    setupBackgroundVertexBuffers();

    initializeObjectProgram();
    setupObjectVertexBuffer();
}

void GLWidget::initializeBackgroundProgram()
{
    QOpenGLShader *vshader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    vshader->compileSourceCode(vertexShaderBackgroundSource);

    QOpenGLShader *fshader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    fshader->compileSourceCode(fragmentShaderBackgroundSource);

    backgroundProgram = new QOpenGLShaderProgram;
    backgroundProgram->addShader(vshader);
    backgroundProgram->addShader(fshader);
    backgroundProgram->bindAttributeLocation("vertex", PROGRAM_VERTEX_ATTRIBUTE);
    backgroundProgram->bindAttributeLocation("texCoord", PROGRAM_TEXCOORD_ATTRIBUTE);
    backgroundProgram->link();

    backgroundProgram->bind();
    backgroundProgram->setUniformValue("texture", 0);
    backgroundProgram->release();
}

void GLWidget::setupBackgroundVertexBuffers()
{
    backgroundProgram->bind();
    backgroundVao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&backgroundVao);

    // Setup our vertex buffer object.
    backgroundVbo.create();
    backgroundVbo.bind();
    backgroundVbo.allocate(backgroundVertexData.constData(), backgroundVertexData.count() * sizeof(GLfloat));

    backgroundVbo.bind();
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glEnableVertexAttribArray(0);
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);
    f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), reinterpret_cast<void *>(3 * sizeof(GLfloat)));
    backgroundVbo.release();
    backgroundProgram->release();
}

void GLWidget::initializeObjectProgram() {
    // Init objects shader program
    m_objectsProgram = new QOpenGLShaderProgram;
    m_objectsProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderObjectSource);
    m_objectsProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderObjectSource);
    m_objectsProgram->bindAttributeLocation("vertex", 0);
    m_objectsProgram->bindAttributeLocation("normal", 1);
    m_objectsProgram->link();


    QMatrix4x4 m_modelMatrix = QMatrix4x4();
    m_modelMatrix.setToIdentity();

    m_viewMatrix = QMatrix4x4(0.99880781f,    0.04439075f, -0.02027142f,  -2.52484405f,
                           -0.01520601f, 0.6778174f, 0.73507287f, 22.31654879f,
                           0.04637038f,  -0.733889f,  0.67768545f,  600.30748785f,
                           0.f,                             0.f,               0.f,              1.f);
    QMatrix4x4 yz_flip;
    yz_flip.setToIdentity();
    yz_flip(1, 1) = -1;
    yz_flip(2, 2) = -1;
    m_viewMatrix = yz_flip * m_viewMatrix;
    m_viewMatrix = m_viewMatrix.transposed();

    // Our camera never changes in this example.
    float k00 = 4781.91740099;
    float k01 = 0.00000000;
    float k02 = 159.66974846999994;
    float k11 = 4778.72123643;
    float k12 = 29.862207509999962;
    float w = (float) this->width();
    float h = (float) this->height();
    float nc = 100.f;
    float fc = 1000.f;
    float depth = (float) fc - nc;
    float q = -(fc + nc) / depth;
    float qn = -2 * (fc * nc) / depth;
    m_projectionMatrix = QMatrix4x4(2 * k00 / w, -2 * k01 / w, (-2 * k02 + w) / w, 0,
                                    0, 2 * k11 / h, (2 * k12 - h) / h, 0,
                                    0,            0,                  q, qn,
                                    0,            0,                 -1, 0);
    m_projectionMatrix = m_projectionMatrix.transposed();
    m_projectionMatrix = m_modelMatrix * m_viewMatrix * m_projectionMatrix;
    m_projectionMatrix = m_projectionMatrix.transposed();
}

void GLWidget::setupObjectVertexBuffer()
{
    // Create a vertex array object. In OpenGL ES 2.0 and OpenGL 2.x
    // implementations this is optional and support may not be present
    // at all. Nonetheless the below code works in all cases and makes
    // sure there is a VAO when one is needed.
    m_objectVao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_objectVao);
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    // Setup the vertex buffer object.
    m_objectModelVertexVbo.create();
    m_objectModelVertexVbo.bind();
    m_objectModelVertexVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_objectModelVertexVbo.allocate(objectModel.getVertices().constData(), objectModel.verticesCount() * sizeof(GLfloat));
    m_objectModelVertexVbo.bind();
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Setup the normal buffer object.
    m_objectModelNormalVbo.create();
    m_objectModelNormalVbo.bind();
    m_objectModelNormalVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_objectModelNormalVbo.allocate(objectModel.getNormals().constData(), objectModel.normalsCount() * sizeof(GLfloat));
    m_objectModelNormalVbo.bind();
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0);

    // Setup the index buffer object.
    m_objectModelIndexVbo.create();
    m_objectModelIndexVbo.bind();
    m_objectModelIndexVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_objectModelIndexVbo.allocate(objectModel.getIndices().constData(), objectModel.indicesCount() * sizeof(GLint));
}

void GLWidget::paintGL()
{   
    glClearColor(clearColor.redF(), clearColor.greenF(), clearColor.blueF(), clearColor.alphaF());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    backgroundProgram->bind();
    {
        QMatrix4x4 m;
        m.ortho(0, 1, 1, 0, 1.0f, 3.0f);
        m.translate(0.0f, 0.0f, -2.0f);

        QOpenGLVertexArrayObject::Binder vaoBinder(&backgroundVao);

        backgroundProgram->setUniformValue("matrix", m);
        backgroundTexture->bind();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    backgroundProgram->release();

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_objectsProgram->bind();
    {
        m_projectionMatrixLoc = m_objectsProgram->uniformLocation("projectionMatrix");
        m_normalMatrixLoc = m_objectsProgram->uniformLocation("normalMatrix");
        m_lightPosLoc = m_objectsProgram->uniformLocation("lightPos");

        QOpenGLVertexArrayObject::Binder vaoBinder(&m_objectVao);

        // Light position is fixed.
        m_objectsProgram->setUniformValue(m_lightPosLoc, QVector3D(0, 0, 70));

        m_objectsProgram->setUniformValue(m_projectionMatrixLoc, m_projectionMatrix);
        QMatrix3x3 normalMatrix = m_viewMatrix.normalMatrix();
        m_objectsProgram->setUniformValue(m_normalMatrixLoc, normalMatrix);

        glDrawElements(GL_TRIANGLES, objectModel.indicesCount(), GL_UNSIGNED_INT, 0);
    }
    m_objectsProgram->release();
}
void GLWidget::resizeGL(int width, int height)
{
    m_proj.setToIdentity();
    // We don't allow the user to resize the window
    //m_proj.perspective(45.0f, GLfloat(width) / height, 0.01f, 1000.0f);
    glViewport(0, 0, width, height);
}

void GLWidget::mousePressEvent(QMouseEvent *event)
{
    lastPos = event->pos();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - lastPos.x();
    int dy = event->y() - lastPos.y();

    if (event->buttons() & Qt::LeftButton) {
        setXRotation(xRot + 8 * dy);
        setYRotation(yRot + 8 * dx);
    } else if (event->buttons() & Qt::RightButton) {
        setXRotation(xRot + 8 * dy);
        setZRotation(zRot + 8 * dx);
    }
    lastPos = event->pos();
}

void GLWidget::mouseReleaseEvent(QMouseEvent * /* event */)
{
    emit clicked();
}

void GLWidget::makeBackgroundObject()
{
    static const int coords[4][3] = {
         { +1, 0, 0 }, { 0, 0, 0 }, { 0, +1, 0 }, { +1, +1, 0 }
    };

    QImage textureImage = QImage(QUrl::fromLocalFile("/home/floretti/git/flowerpower_nn/data/assets/tless/train_canon/01/generated/images/1002.jpg").path()).mirrored();
    backgroundTexture = new QOpenGLTexture(textureImage);
    backgroundTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
    backgroundTexture->setMinificationFilter(QOpenGLTexture::Nearest);

    for (int i = 0; i < 4; ++i) {
        // vertex position
        backgroundVertexData.append(coords[i][0]);
        backgroundVertexData.append(coords[i][1]);
        backgroundVertexData.append(coords[i][2]);
        // texture coordinate
        backgroundVertexData.append(i == 0 || i == 3);
        backgroundVertexData.append(i == 0 || i == 1);
    }
}
