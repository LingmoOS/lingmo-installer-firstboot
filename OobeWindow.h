#ifndef OOBEWINDOW_H
#define OOBEWINDOW_H

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>

class OobeWindow : public QWidget
{
    Q_OBJECT
public:
    explicit OobeWindow(QWidget *parent = nullptr);

private slots:
    void nextPage();
    void backPage();
    void applySettings();

private:
    void setupBackground();
    void fadeToPage(int index);
    QWidget* createCard();
    QWidget* createWelcomePage();
    QWidget* createHostnamePage();
    QWidget* createApplyingPage();
    QWidget* createFinishedPage();
    void updateNavButtons();

    QStackedWidget *m_stack;
    QPushButton *m_backBtn;
    QPushButton *m_nextBtn;

    // Personalization data
    QString m_hostname;

    // Pages
    QLineEdit *m_hostnameEdit;
    QLabel    *m_hostnameError;
    QLabel    *m_finishDesc;

    QWidget *m_cardWidget;
    int m_cardWidth = 0;
    QLabel *m_progressLabel;
    QWidget *m_progressBar;
};

#endif
