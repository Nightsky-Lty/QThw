#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QVector>
#include <QMap>
#include "hardwaremodule.h"
#include "hardwarevisualizer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void resetToInitial();

private:
    void createToolBar();
    void createActions();
    void setupInitialLayout();
    void loadConfiguration();
    
    // 从配置文件加载硬件配置
    void loadSetupFile(const QString& filename);
    // 从统计文件加载性能数据
    void loadStatisticFile(const QString& filename);
    // 更新模块统计信息
    void updateModuleStatistics(const QString& moduleName, const QMap<QString, double>& stats);

    HardwareVisualizer *m_visualizer;
    QToolBar *m_toolBar;
    QVector<HardwareModule*> m_modules;
    QMap<QString, HardwareModule*> m_moduleMap; // 模块名到模块指针的映射

    // 工具栏动作
    QAction *m_resetAction;
    QAction *m_drawLineAction;
    QAction *m_themeAction;
    
    // 主题状态
    bool m_darkTheme;
};

#endif // MAINWINDOW_H 