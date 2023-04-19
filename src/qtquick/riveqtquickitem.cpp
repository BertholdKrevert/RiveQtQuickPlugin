/*
 * MIT License
 *
 * Copyright (C) 2023 by Jeremias Bosch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "riveqtquickitem.h"
#include "qqmlcontext.h"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/file.hpp"
#include "riveqtfactory.h"
#include "riveqtopenglrenderer.h"

#include <QSGRenderNode>
#include <QQmlEngine>

#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/image.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/generated/shapes/shape_base.hpp>

RiveQtQuickItem::RiveQtQuickItem(QQuickItem *parent)
  : QQuickItem(parent)
{
  // set global flags and configs of our item
  // TODO: maybe we should also allow Hover Events to be used
  setFlag(QQuickItem::ItemHasContents, true);
  setAcceptedMouseButtons(Qt::AllButtons);

  // TODO: 1) shall we make this Interval match the FPS of the current selected animation
  // TODO: 2) we may want to move this into the render thread to allow the render thread control over the timer, the timer itself only
  // triggers Updates
  //          all animations are done during the node update, from within the renderthread
  //          Note: this is kind of scarry as all objects are created in the main thread
  //                which means that we have to be really carefull here to not crash

  // those will be triggered by renderthread once an update was applied to the pipeline
  // this will inform QML about changes
  connect(this, &RiveQtQuickItem::internalArtboardChanged, this, &RiveQtQuickItem::currentArtboardIndexChanged, Qt::QueuedConnection);
  connect(this, &RiveQtQuickItem::internalArtboardChanged, this, &RiveQtQuickItem::updateAnimations, Qt::QueuedConnection);
  connect(this, &RiveQtQuickItem::internalArtboardChanged, this, &RiveQtQuickItem::updateStateMachines, Qt::QueuedConnection);
  connect(
    this, &RiveQtQuickItem::internalStateMachineChanged, this,
    [this]() {
      if (m_stateMachineInputMap) {
        m_stateMachineInputMap->deleteLater();
      }
      // maybe its a bit maniac and insane to push raw instance pointers around.
      // well what could go wrong. aka TODO: dont do this
      m_stateMachineInputMap = new RiveQtStateMachineInputMap(m_currentStateMachineInstance.get(), this);
      emit stateMachineInterfaceChanged();
    },
    Qt::QueuedConnection);

  // do update the index only once we are setuped and happy
  connect(this, &RiveQtQuickItem::stateMachineInterfaceChanged, this, &RiveQtQuickItem::currentStateMachineIndexChanged,
          Qt::QueuedConnection);

  m_elapsedTimer.start();
  m_lastUpdateTime = m_elapsedTimer.elapsed();

  update();
}

// void RiveQtQuickItem::paint(QPainter* painter)
//{
//     if (!m_artboardInstance || !painter)
//     {
//         return;
//     }

//    m_renderer.setPainter(painter);
//    m_artboardInstance->draw(&m_renderer);

//    qDebug("paint");
//}

void RiveQtQuickItem::triggerAnimation(int id)
{
  if (m_currentArtboardInstance) {
    if (id >= 0 && id < m_currentArtboardInstance->animationCount()) {
      m_animationInstance = m_currentArtboardInstance->animationAt(id);
    }
  }
}

QSGNode *RiveQtQuickItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
  RiveQSGRenderNode *node = static_cast<RiveQSGRenderNode *>(oldNode);

  if (m_scheduleArtboardChange) {
    m_currentArtboardInstance = m_riveFile->artboardAt(m_currentArtboardIndex);
    if (m_currentArtboardInstance) {
      m_animationInstance = m_currentArtboardInstance->animationNamed(m_currentArtboardInstance->firstAnimation()->name());
      m_currentArtboardInstance->updateComponents();
      if (m_currentStateMachineIndex == -1) {
        setCurrentStateMachineIndex(m_currentArtboardInstance->defaultStateMachineIndex()); // this will set m_scheduleStateMachineChange
      }
    }
    emit internalArtboardChanged();

    if (node) {
      node->updateArtboardInstance(m_currentArtboardInstance.get());
    }

    m_scheduleArtboardChange = false;
  }

  if (m_scheduleStateMachineChange && m_currentArtboardInstance) {
    m_currentStateMachineInstance = m_currentArtboardInstance->stateMachineAt(m_currentStateMachineIndex);
    emit internalStateMachineChanged();
    m_scheduleStateMachineChange = false;
  }

  if (!node && m_currentArtboardInstance) {
    node = new RiveQSGRenderNode(m_currentArtboardInstance.get(), this);
  }

  qint64 currentTime = m_elapsedTimer.elapsed();
  float deltaTime = (currentTime - m_lastUpdateTime) / 1000.0f;
  m_lastUpdateTime = currentTime;

  if (m_currentArtboardInstance) {
    if (m_animationInstance) {
      bool shouldContinue = m_animationInstance->advance(deltaTime);
      if (shouldContinue) {
        m_animationInstance->apply();
      }
    }
    if (m_currentStateMachineInstance) {
      m_currentStateMachineInstance->advance(deltaTime);
    }
    m_currentArtboardInstance->updateComponents();
    m_currentArtboardInstance->advance(deltaTime);
    m_currentArtboardInstance->update(rive::ComponentDirt::Filthy);
  }

  if (node) {
    node->markDirty(QSGNode::DirtyForceUpdate);
  }
  this->update();

  return node;
}

void RiveQtQuickItem::mousePressEvent(QMouseEvent *event)
{
  hitTest(event->pos(), rive::ListenerType::down);
  // todo: shall we tell qml about a hit and details about that?
}

void RiveQtQuickItem::mouseMoveEvent(QMouseEvent *event)
{
  hitTest(event->pos(), rive::ListenerType::move);
  // todo: shall we tell qml about a hit and details about that?
}

void RiveQtQuickItem::mouseReleaseEvent(QMouseEvent *event)
{
  hitTest(event->pos(), rive::ListenerType::up);
  // todo: shall we tell qml about a hit and details about that?
}

void RiveQtQuickItem::setFileSource(const QString &source)
{
  if (m_fileSource != source) {
    m_fileSource = source;
    emit fileSourceChanged();

    // Load the Rive file when the fileSource is set
    loadRiveFile(source);
  }
}

void RiveQtQuickItem::loadRiveFile(const QString &source)
{
  if (source.isEmpty()) {
    return;
  }

  m_loadingStatus = Loading;
  emit loadingStatusChanged();

  QFile file(source);

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning("Failed to open the file.");
    m_loadingStatus = Error;
    emit loadingStatusChanged();
    return;
  }

  QByteArray fileData = file.readAll();

  rive::Span<const uint8_t> dataSpan(reinterpret_cast<const uint8_t *>(fileData.constData()), fileData.size());

  rive::ImportResult importResult;
  m_riveFile = rive::File::import(dataSpan, &customFactory, &importResult);

  if (importResult == rive::ImportResult::success) {
    qDebug("Successfully imported Rive file.");
    m_loadingStatus = Loaded;

    // Get info about the artboards
    for (size_t i = 0; i < m_riveFile->artboardCount(); i++) {
      const auto artBoard = m_riveFile->artboard(i);

      if (artBoard) {
        ArtBoardInfo info;
        info.id = i;
        info.name = QString::fromStdString(m_riveFile->artboardNameAt(i));
        m_artboards.append(info);
      }
    }

    emit artboardsChanged();

    // todo allow to preselect the artboard and statemachine and animation at load
    setCurrentArtboardIndex(0);
    if (m_initialStateMachineIndex != -1) {
      setCurrentStateMachineIndex(m_initialStateMachineIndex);
    }
  } else {
    qDebug("Failed to import Rive file.");
    m_loadingStatus = Error;
  }
  emit loadingStatusChanged();
}

void RiveQtQuickItem::updateAnimations()
{
  m_animationList.clear();

  if (!m_currentArtboardInstance) {
    return;
  }

  for (size_t i = 0; i < m_currentArtboardInstance->animationCount(); i++) {
    const auto animation = m_currentArtboardInstance->animation(i);
    if (animation) {
      qDebug() << "Animation" << i << "found.";

      qDebug() << "  Duration:" << animation->duration();
      qDebug() << "  FPS:" << animation->fps();

      AnimationInfo info;
      info.id = i;
      info.name = QString::fromStdString(animation->name());
      info.duration = animation->duration();
      info.fps = animation->fps();

      qDebug() << "  Name:" << info.name;
      m_animationList.append(info);
    }
  }

  emit animationsChanged();
}

void RiveQtQuickItem::updateStateMachines()
{
  m_stateMachineList.clear();

  if (!m_currentArtboardInstance) {
    return;
  }

  for (size_t i = 0; i < m_currentArtboardInstance->stateMachineCount(); i++) {
    const auto stateMachine = m_currentArtboardInstance->stateMachine(i);
    if (stateMachine) {
      StateMachineInfo info;
      info.id = i;
      info.name = QString::fromStdString(stateMachine->name());

      m_stateMachineList.append(info);
    }
  }
  emit stateMachinesChanged();
}

bool RiveQtQuickItem::hitTest(const QPointF &pos, const rive::ListenerType &type)
{
  if (!m_riveFile || !m_currentArtboardInstance || !m_currentStateMachineInstance)
    return false;

  // Scale mouse position based on current item size and artboard size
  float scaleX = width() / m_currentArtboardInstance->width();
  float scaleY = height() / m_currentArtboardInstance->height();

  // Calculate the uniform scale factor to preserve the aspect ratio
  auto scaleFactor = qMin(scaleX, scaleY);

  // Calculate the new width and height of the item while preserving the aspect ratio
  auto newWidth = m_currentArtboardInstance->width() * scaleFactor;
  auto newHeight = m_currentArtboardInstance->height() * scaleFactor;

  // Calculate the offsets needed to center the item within its bounding rectangle
  auto offsetX = (width() - newWidth) / 2.0;
  auto offsetY = (height() - newHeight) / 2.0;

  rive::HitInfo hitInfo;
  rive::Mat2D transform;

  float x = (pos.x() - offsetX) / scaleFactor;
  float y = (pos.y() - offsetY) / scaleFactor;

  for (int i = 0; i < m_currentStateMachineInstance->stateMachine()->listenerCount(); ++i) {
    auto *listener = m_currentStateMachineInstance->stateMachine()->listener(i);
    if (listener) {
      if (listener->listenerType() == rive::ListenerType::enter || listener->listenerType() == rive::ListenerType::exit) {
        qDebug() << "Enter and Exit Actions are not yet supported";
      }

      if (listener->listenerType() == type) {
        for (const auto &id : listener->hitShapeIds()) {
          auto *coreElement = m_currentStateMachineInstance->artboard()->resolve(id);

          if (coreElement->is<rive::Shape>()) {
            rive::Shape *shape = dynamic_cast<rive::Shape *>(coreElement);

            const rive::IAABB area = { x, y, x + 1, y + 1 };
            bool hit = shape->hitTest(area);

            if (hit) {
              listener->performChanges(m_currentStateMachineInstance.get(), rive::Vec2D(x, y));
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

RiveQSGRenderNode::RiveQSGRenderNode(rive::ArtboardInstance *artboardInstance, RiveQtQuickItem *item)
  : m_artboardInstance(artboardInstance)
  , m_item(item)
{
  initializeOpenGLFunctions();
  m_renderer.initGL();
}

QPointF RiveQSGRenderNode::globalPosition(QQuickItem *item)
{
  if (!item)
    return QPointF(0, 0);
  QQuickItem *parentItem = item->parentItem();
  if (parentItem) {
    return item->position() + globalPosition(parentItem);
  } else {
    return item->position();
  }
}

void RiveQSGRenderNode::render(const RenderState *state)
{
  // Render the artboard instance using the renderer and OpenGL
  if (m_artboardInstance) {

    QPointF globalPos = globalPosition(m_item);
    auto x = globalPos.x();
    auto y = globalPos.y();

    auto aspectX = m_item->width() / (m_artboardInstance->width());
    auto aspectY = m_item->height() / (m_artboardInstance->height());

    // Calculate the uniform scale factor to preserve the aspect ratio
    auto scaleFactor = qMin(aspectX, aspectY);

    // Calculate the new width and height of the item while preserving the aspect ratio
    auto newWidth = m_artboardInstance->width() * scaleFactor;
    auto newHeight = m_artboardInstance->height() * scaleFactor;

    // Calculate the offsets needed to center the item within its bounding rectangle
    auto offsetX = (m_item->width() - newWidth) / 2.0;
    auto offsetY = (m_item->height() - newHeight) / 2.0;

    QMatrix4x4 modelMatrix;
    modelMatrix.translate(x + offsetX, y + offsetY);
    modelMatrix.scale(scaleFactor, scaleFactor);

    m_renderer.updateViewportSize();
    m_renderer.updateModelMatrix(modelMatrix);
    m_renderer.updateProjectionMatrix(*state->projectionMatrix());

    // TODO: sicssorRect of RenderState is 0x0 width,
    // just create it for now.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int viewportHeight = viewport[3];

    glEnable(GL_SCISSOR_TEST);
    int scissorX = static_cast<int>(x);
    int scissorY = static_cast<int>(viewportHeight - y - m_item->height());
    int scissorWidth = static_cast<int>(m_item->width());
    int scissorHeight = static_cast<int>(m_item->height());
    glScissor(scissorX, scissorY, scissorWidth, scissorHeight);

    // this renders the artboard!
    m_artboardInstance->draw(&m_renderer);

    glDisable(GL_SCISSOR_TEST);
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

QRectF RiveQSGRenderNode::rect() const
{
  // Return the bounding rectangle of your item
  // You should update m_rect with the actual bounding rectangle
  return m_item->boundingRect();
}

const QList<AnimationInfo> &RiveQtQuickItem::animations() const
{
  return m_animationList;
}

const QList<ArtBoardInfo> &RiveQtQuickItem::artboards() const
{
  return m_artboards;
}

int RiveQtQuickItem::currentArtboardIndex() const
{
  return m_currentArtboardIndex;
}

void RiveQtQuickItem::setCurrentArtboardIndex(int newCurrentArtboardIndex)
{
  if (m_currentArtboardIndex == newCurrentArtboardIndex) {
    return;
  }

  if (!m_riveFile) {
    return;
  }

  if (newCurrentArtboardIndex < 0 || newCurrentArtboardIndex >= m_riveFile->artboardCount()) {
    return;
  }

  m_currentArtboardIndex = newCurrentArtboardIndex;

  m_scheduleArtboardChange = true; // we have to do this in the render thread.

  update();
}

int RiveQtQuickItem::currentStateMachineIndex() const
{
  return m_currentStateMachineIndex;
}

void RiveQtQuickItem::setCurrentStateMachineIndex(int newCurrentStateMachineIndex)
{
  if (m_currentStateMachineIndex == newCurrentStateMachineIndex) {
    return;
  }

  if (!m_riveFile) {
    m_initialStateMachineIndex = newCurrentStateMachineIndex; // file not yet loaded, save the info from qml
    return;
  }

  if (!m_riveFile->artboard())
    return;

  // -1 is a valid value!
  if (newCurrentStateMachineIndex != -1) {
    if (newCurrentStateMachineIndex >= m_riveFile->artboard()->stateMachineCount()) {
      return;
    }
  }

  m_currentStateMachineIndex = newCurrentStateMachineIndex;

  m_scheduleStateMachineChange = true; // we have to do this in the render thread.
  update();
}

RiveQtStateMachineInputMap *RiveQtQuickItem::stateMachineInterface() const
{
  return m_stateMachineInputMap;
}

bool RiveQtQuickItem::interactive() const
{
  return acceptedMouseButtons() == Qt::AllButtons;
}

void RiveQtQuickItem::setInteractive(bool newInteractive)
{
  if (acceptedMouseButtons() == Qt::AllButtons && newInteractive || (acceptedMouseButtons() != Qt::AllButtons && !newInteractive)) {
    return;
  }

  if (newInteractive) {
    setAcceptedMouseButtons(Qt::AllButtons);
  }

  if (!newInteractive) {
    setAcceptedMouseButtons(Qt::NoButton);
  }

  emit interactiveChanged();
}
