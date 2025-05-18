#include "mainwindow.h"
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , clickCount(0)
{
    // 创建中央部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建布局
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // 创建按钮
    button = new QPushButton("Click Me!", this);
    layout->addWidget(button);

    // 创建标签
    label = new QLabel("Button clicked 0 times", this);
    layout->addWidget(label);

    // 连接信号和槽
    connect(button, &QPushButton::clicked, this, &MainWindow::onButtonClicked);

    // 设置窗口属性
    setWindowTitle("My First Qt Application");
    resize(400, 300);
}

MainWindow::~MainWindow()
{
}

void MainWindow::onButtonClicked()
{
    clickCount++;
    label->setText(QString("Button clicked %1 times").arg(clickCount));
} 