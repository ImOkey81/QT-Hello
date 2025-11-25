#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Создаем виджет для отображения текста
    QLabel *label = new QLabel("Hello World!", this);
    label->setAlignment(Qt::AlignCenter);

    // Устанавливаем шрифт
    QFont font = label->font();
    font.setPointSize(20);
    font.setBold(true);
    label->setFont(font);

    // Добавляем в центральный виджет через layout
    QVBoxLayout *layout = new QVBoxLayout(ui->centralwidget);
    layout->addWidget(label);
}

MainWindow::~MainWindow()
{
    delete ui;
}
