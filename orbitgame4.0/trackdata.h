
#ifndef TRACKDATA_H
#define TRACKDATA_H

#include <vector>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>

    // 前向声明 (如果您的结构体定义顺序可能导致问题，但在此简单场景下通常不需要)
    // struct CollectibleData;
    // struct ObstacleData;
    // struct TrackSegmentData;

    // 定义收集品的数据结构
    struct CollectibleData {
    double angleDegrees; // 收集品在轨道上的角度（度数）
    double radialOffset; // 收集品相对于轨道半径的径向偏移量

    // 从JSON对象加载数据
    static CollectibleData fromJsonObject(const QJsonObject& json) {
        CollectibleData data;
        data.angleDegrees = json["angleDegrees"].toDouble();
        // 如果JSON中没有radialOffset，默认为0 (即在轨道线上)
        data.radialOffset = json.contains("radialOffset") ? json["radialOffset"].toDouble() : 0.0;
        // qDebug() << "Parsed Collectible: angle=" << data.angleDegrees << "offset=" << data.radialOffset;
        return data;
    }
};

// 定义障碍物的数据结构
struct ObstacleData {
    double angleDegrees; // 障碍物在轨道上的角度（度数）
    double radialOffset; // 障碍物相对于轨道半径的径向偏移量

    // 从JSON对象加载数据
    static ObstacleData fromJsonObject(const QJsonObject& json) {
        ObstacleData data;
        data.angleDegrees = json["angleDegrees"].toDouble();
        // 如果JSON中没有radialOffset，默认为0 (即在轨道线上)
        data.radialOffset = json.contains("radialOffset") ? json["radialOffset"].toDouble() : 0.0;
        // qDebug() << "Parsed Obstacle: angle=" << data.angleDegrees << "offset=" << data.radialOffset;
        return data;
    }
};

// 定义轨道段的数据结构
struct TrackSegmentData {
    double centerX;          // 轨道段的中心X坐标
    double centerY;          // 轨道段的中心Y坐标
    double radius;           // 轨道段的半径
    double tangentAngleDegrees; // 轨道段的切线角度 (目前在您的JSON中为0)
    std::vector<CollectibleData> collectibles; // 该轨道段上的收集品列表
    std::vector<ObstacleData> obstacles;     // 该轨道段上的障碍物列表

    // 从JSON对象加载数据
    static TrackSegmentData fromJsonObject(const QJsonObject& json) {
        TrackSegmentData data;
        data.centerX = json["centerX"].toDouble();
        data.centerY = json["centerY"].toDouble();
        data.radius = json["radius"].toDouble();
        data.tangentAngleDegrees = json.contains("tangentAngleDegrees") ? json["tangentAngleDegrees"].toDouble() : 0.0;
        // qDebug() << "Parsed TrackSegment: centerX=" << data.centerX << "centerY=" << data.centerY << "radius=" << data.radius;

        if (json.contains("collectibles") && json["collectibles"].isArray()) {
            QJsonArray collectiblesArray = json["collectibles"].toArray();
            for (const QJsonValue& val : collectiblesArray) {
                if (val.isObject()) {
                    data.collectibles.push_back(CollectibleData::fromJsonObject(val.toObject()));
                }
            }
        }

        if (json.contains("obstacles") && json["obstacles"].isArray()) {
            QJsonArray obstaclesArray = json["obstacles"].toArray();
            for (const QJsonValue& val : obstaclesArray) {
                if (val.isObject()) {
                    data.obstacles.push_back(ObstacleData::fromJsonObject(val.toObject()));
                }
            }
        }
        return data;
    }
};

// TrackData 类用于加载和管理所有轨道段数据
class TrackData {
public:
    std::vector<TrackSegmentData> segments; // 存储所有轨道段的列表

    // 默认构造函数
    TrackData() = default;

    // 从JSON字符串加载关卡数据
    // 参数: jsonString - 包含关卡数据的JSON格式字符串
    // 返回: 如果加载成功则为true，否则为false
    bool loadLevelFromJson(const QString& jsonString) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);

        if (doc.isNull()) {
            qWarning() << "Failed to parse JSON: Document is null. Error:" << parseError.errorString()
            << "at offset" << parseError.offset;
            return false;
        }
        if (!doc.isArray()) {
            qWarning() << "Failed to parse JSON: Document is not an array.";
            return false;
        }

        segments.clear(); // 清除旧数据
        QJsonArray levelArray = doc.array();

        if (levelArray.isEmpty()) {
            qWarning() << "JSON array for level segments is empty. Loading as an empty level.";
            // 允许空关卡, 如果返回false则空关卡加载失败
        }

        for (const QJsonValue& segmentValue : levelArray) {
            if (segmentValue.isObject()) {
                segments.push_back(TrackSegmentData::fromJsonObject(segmentValue.toObject()));
            } else {
                qWarning() << "Encountered non-object value in segments array. Skipping.";
            }
        }
        qDebug() << "Successfully loaded" << segments.size() << "track segments from JSON string.";
        return true;
    }

    // 从文件加载关卡数据
    // 参数: filePath - 关卡JSON文件的路径
    // 返回: 如果加载成功则为true，否则为false
    bool loadLevelFromFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Couldn't open level file:" << filePath << "Error:" << file.errorString();
            return false;
        }
        QString jsonData = file.readAll();
        file.close();

        // 检查读取到的jsonData是否为空
        if (jsonData.isEmpty()) {
            if (file.size() > 0) {
                qWarning() << "Read empty data from a non-empty file:" << filePath;
                return false; // 从非空文件读取到空数据，可能是一个错误
            } else {
                qWarning() << "Level file is empty:" << filePath << ". Loading as an empty level.";
                segments.clear(); // 确保如果是空文件，segments也是空的
                return true; // 视为空关卡，加载成功
            }
        }
        qDebug() << "Loading level from file:" << filePath;
        return loadLevelFromJson(jsonData);
    }
};

#endif
