#ifndef MODULEINFODIALOG_H
#define MODULEINFODIALOG_H

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QGraphicsItem>
#include "hardwaremodule.h"

class ModuleInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModuleInfoDialog(HardwareModule* module, const QMap<HardwareModule*, QGraphicsItem*>& moduleItems, QWidget* parent = nullptr);

private:
    void setupUI();
    void updateModuleInfo();
    QString formatStatistic(const QString& key, double value) const;
    QString getModuleTypeName(HardwareModule::ModuleType type) const;
    QString getConnectionInfo() const;

    HardwareModule* m_module;
    const QMap<HardwareModule*, QGraphicsItem*>& m_moduleItems;
    QTextBrowser* m_textBrowser;
};

#endif // MODULEINFODIALOG_H 