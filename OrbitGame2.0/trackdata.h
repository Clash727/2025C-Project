// trackdata.h
#ifndef TRACKDATA_H
#define TRACKDATA_H

#include <QPointF>
#include <QRectF>

// 用于保存轨道信息的简单结构体
struct TrackData {
    QPointF center;      // 轨道中心坐标
    qreal radius = 0.0;  // 轨道半径
    qreal tangentAngle = 0.0; // 在 *此* 轨道上，*下一个* 轨道接触点的角度（弧度）
    QRectF bounds;       // 用于绘制/计算的边界矩形
    // QList<qreal> collectibleAngles; // 已移除

    TrackData() = default; // 默认构造函数

    TrackData(QPointF c, qreal r) : center(c), radius(r) {
        bounds = QRectF(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);
    }
};

#endif // TRACKDATA_H
