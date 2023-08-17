// SPDX-FileCopyrightText: 2023 Jeremias Bosch <jeremias.bosch@basyskom.com>
// SPDX-FileCopyrightText: 2023 Jonas Kalinka <jonas.kalinka@basyskom.com>
// SPDX-FileCopyrightText: 2023 basysKom GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include <optional>

#include <QPainterPath>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector>
#include <QPen>

#include <rive/renderer.hpp>
#include <rive/math/raw_path.hpp>

class Test_PathTriangulation;

class RiveQtPath : public rive::RenderPath
{
public:
    RiveQtPath(const unsigned segmentCount);
    RiveQtPath(const RiveQtPath &other);
    RiveQtPath(const rive::RawPath &rawPath, rive::FillRule fillRule, const unsigned segmentCount);

    void rewind() override;
    void moveTo(float x, float y) override { m_qPainterPath.moveTo(x, y); }
    void lineTo(float x, float y) override { m_qPainterPath.lineTo(x, y); }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override { m_qPainterPath.cubicTo(ox, oy, ix, iy, x, y); }
    void close() override { m_qPainterPath.closeSubpath(); }
    void fillRule(rive::FillRule value) override;
    void addRenderPath(rive::RenderPath *path, const rive::Mat2D &transform) override;

    void setQPainterPath(QPainterPath path);
    QPainterPath toQPainterPath() const { return m_qPainterPath; }
    QPainterPath toQPainterPaths(const QMatrix4x4 &t);

    void setSegmentCount(const unsigned segmentCount);

    QVector<QVector<QVector2D>> toVertices();
    QVector<QVector<QVector2D>> toVerticesLine(const QPen &pen);

private:
    struct PathDataPoint
    {
        QVector2D point;
        QVector2D tangent; // only used in cubic bezier
        int stepIndex;
    };

    static bool doTrianglesOverlap(const QVector2D &p1, const QVector2D &p2, const QVector2D &p3, const QVector2D &p4, const QVector2D &p5,
                                   const QVector2D &p6);

    static QVector<std::pair<size_t, size_t>> findOverlappingTriangles(const QVector<QVector2D> &trianglePoints);
    static void concaveHull(const QVector<QVector2D> &t1, const QVector<QVector2D> &t2, QVector<QVector2D> &result, const size_t i = 0, const size_t counter = 0);

    static void maybeAdd(const QVector2D &item, QVector<QVector2D> &result);

    static std::optional<QVector2D> calculateLineIntersection(const QVector2D &p1, const QVector2D &p2, const QVector2D &p3,
                                                              const QVector2D &p4);
    static void removeOverlappingTriangles(QVector<QVector2D> &triangles);
    static bool isInsidePolygon(const QVector<QVector2D> &polygon, const QVector2D &p);

    static QPointF cubicBezier(const QPointF &startPoint, const QPointF &controlPoint1, const QPointF &controlPoint2,
                               const QPointF &endPoint, qreal t);

    static int orientation(const QVector<QVector2D> &points);
    static void FixWinding(QVector2D &p1, QVector2D &p2, QVector2D &p3);

    void updatePathSegmentsData();
    void updatePathSegmentsOutlineData();
    void updatePathOutlineVertices(const QPen &pen);

    QPainterPath m_qPainterPath;
    QVector<QVector<PathDataPoint>> m_pathSegmentsOutlineData;

    QVector<QVector<QVector2D>> m_pathVertices;
    QVector<QVector<QVector2D>> m_pathOutlineVertices;

    bool m_pathSegmentDataDirty { true };
    bool m_pathSegmentOutlineDataDirty { true };
    unsigned m_segmentCount { 10 };

    friend Test_PathTriangulation;
};
