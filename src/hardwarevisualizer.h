#ifndef HARDWAREVISUALIZER_H
#define HARDWAREVISUALIZER_H

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMap>
#include <QColor>
#include "hardwaremodule.h"

class HardwareVisualizer : public QGraphicsView
{
    Q_OBJECT

public:
    explicit HardwareVisualizer(QWidget *parent = nullptr);
    ~HardwareVisualizer();

    // 添加硬件模块到场景
    void addModule(HardwareModule* module);
    // 更新模块位置
    void updateModulePosition(HardwareModule* module);
    // 清除所有模块
    void clearModules();
    // 自动布局所有模块
    void autoLayout();

protected:
    // 处理鼠标事件，用于拖拽模块
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    // 处理缩放事件
    void wheelEvent(QWheelEvent *event) override;

private:
    QGraphicsScene *m_scene;
    QMap<HardwareModule*, QGraphicsItem*> m_moduleItems;
    QGraphicsItem* m_draggedItem;
    QPointF m_lastMousePos;
    HardwareModule* m_busModule;  // 保存总线模块的引用

    // 创建不同类型硬件模块的图形项
    QGraphicsItem* createModuleItem(HardwareModule* module);
    // 绘制模块之间的连接
    void drawConnections();
    // 更新模块的统计信息显示
    void updateStatistics(HardwareModule* module);
    // 获取模块颜色
    QColor getModuleColor(HardwareModule::ModuleType type) const;
    // 获取模块类型名称
    QString getModuleTypeName(HardwareModule::ModuleType type) const;
    // 创建统计信息文本
    QString createStatsText(HardwareModule* module) const;
    // 计算模块的自动布局位置
    QPointF calculateModulePosition(HardwareModule* module, int nodeId) const;
    // 获取两个模块之间的数据传输量
    double getDataTransferRate(HardwareModule* from, HardwareModule* to) const;
    // 格式化统计信息
    QString formatStatistic(const QString& key, double value) const;
};

#endif // HARDWAREVISUALIZER_H 