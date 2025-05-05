#ifndef TRACKDATA_H
#define TRACKDATA_H

#include <QPointF>
#include <QRectF>

// Simple structure to hold information about a track
struct TrackData {
    QPointF center;      // Center coordinates of the track
    qreal radius = 0.0;  // Radius of the track
    qreal tangentAngle = 0.0; // Angle (radians) on *this* track where the *next* track touches
    QRectF bounds;       // Bounding rect for drawing/calculations

    TrackData() = default; // Default constructor

    TrackData(QPointF c, qreal r) : center(c), radius(r) {
        bounds = QRectF(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);
    }
};

#endif // TRACKDATA_H
