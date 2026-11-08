#include "OobeWindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFile>
#include <QGraphicsBlurEffect>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QScreen>
#include <QStyle>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

static const double CARD_RATIO = 0.60;
static const int CARD_MIN_W = 720;
static const int CARD_MAX_W = 1100;
static const int CARD_MARGIN = 80;
static const double CARD_MAX_H_RATIO = 0.86;
static const int LEFT_MIN_W = 220;
static const int LEFT_MAX_W = 420;
static const int LEFT_HIDE_BELOW_W = 800;
static const int PAGE_PADDING = 40;
static const char* OOBE_FLAG = "/etc/.firstboot-complete";

static QString osReleaseField(const QString &key)
{
    const char *paths[] = { "/usr/lib/os-release", "/etc/os-release" };
    for (const char *path : paths) {
        QFile f(QString::fromLatin1(path));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (!line.startsWith(key + QLatin1Char('=')))
                continue;
            QString value = line.mid(key.length() + 1);
            if (value.length() >= 2 && value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                value = value.mid(1, value.length() - 2);
            return value;
        }
    }
    return QString();
}

OobeWindow::OobeWindow(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("oobeWindow");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    QSize screenSize = qApp->primaryScreen()->availableGeometry().size();
    setGeometry(0, 0, screenSize.width(), screenSize.height());

    setupBackground();

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addStretch();
    {
        QHBoxLayout *hrow = new QHBoxLayout;
        hrow->addStretch();
        hrow->addWidget(createCard());
        hrow->addStretch();
        root->addLayout(hrow);
    }
    root->addStretch();

    QMetaObject::invokeMethod(this, "showFullScreen", Qt::QueuedConnection);

    auto *entranceEffect = new QGraphicsOpacityEffect(m_cardWidget);
    m_cardWidget->setGraphicsEffect(entranceEffect);
    entranceEffect->setOpacity(0.0);
    auto *entranceAnim = new QPropertyAnimation(entranceEffect, "opacity", this);
    entranceAnim->setDuration(400);
    entranceAnim->setStartValue(0.0);
    entranceAnim->setEndValue(1.0);
    entranceAnim->setEasingCurve(QEasingCurve::OutCubic);
    entranceAnim->start(QAbstractAnimation::DeleteWhenStopped);
    connect(entranceAnim, &QPropertyAnimation::finished, this, [this]() {
        m_cardWidget->setGraphicsEffect(nullptr);
    });
}

void OobeWindow::setupBackground()
{
    QSize screenSize = size();

    QLabel *overlay = new QLabel(this);
    overlay->setGeometry(rect());
    overlay->setStyleSheet("background: rgba(0, 0, 0, 0.3);");

    QLabel *bg = new QLabel(this);
    bg->setGeometry(rect());
    bg->setObjectName("oobeBg");

    QStringList candidates = {
        "/usr/share/backgrounds/lingmo/default.png",
        "/usr/share/backgrounds/default.png",
        "/usr/share/wallpapers/Deepin/contents/images/default.jpg"
    };

    QPixmap bgPix;
    for (const auto &path : candidates) {
        if (bgPix.load(path)) break;
    }

    if (!bgPix.isNull()) {
        bgPix = bgPix.scaled(screenSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int x = (bgPix.width() - screenSize.width()) / 2;
        int y = (bgPix.height() - screenSize.height()) / 2;
        bgPix = bgPix.copy(qMax(0, x), qMax(0, y), screenSize.width(), screenSize.height());
        bg->setPixmap(bgPix);

        auto *blur = new QGraphicsBlurEffect(bg);
        blur->setBlurRadius(40.0);
        blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
        bg->setGraphicsEffect(blur);
    } else {
        QPixmap gradient(screenSize);
        QPainter p(&gradient);
        QLinearGradient g(0, 0, 0, screenSize.height());
        g.setColorAt(0.0, QColor("#0f0c29"));
        g.setColorAt(0.5, QColor("#302b63"));
        g.setColorAt(1.0, QColor("#24243e"));
        p.fillRect(gradient.rect(), g);
        p.end();
        bg->setPixmap(gradient);
    }

    bg->lower();
    overlay->lower();
    bg->show();
    overlay->show();
}

QWidget* OobeWindow::createCard()
{
    QSize screenSize = qApp->primaryScreen()->availableGeometry().size();

    const int maxW = qMin(CARD_MAX_W, screenSize.width() - CARD_MARGIN);
    const int minW = qMin(CARD_MIN_W, maxW);
    m_cardWidth = qBound(minW,
                         static_cast<int>(screenSize.width() * CARD_RATIO),
                         maxW);

    m_cardWidget = new QWidget(this);
    m_cardWidget->setObjectName("oobeCard");
    m_cardWidget->setFixedWidth(m_cardWidth);
    m_cardWidget->setMaximumHeight(static_cast<int>(screenSize.height() * CARD_MAX_H_RATIO));

    QVBoxLayout *cardLayout = new QVBoxLayout(m_cardWidget);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    QWidget *header = new QWidget;
    header->setObjectName("cardHeader");
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(24, 18, 24, 10);

    QLabel *logo = new QLabel;
    QPixmap lpix(":/logo.png");
    if (!lpix.isNull())
        logo->setPixmap(lpix.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    hl->addWidget(logo);

    QLabel *title = new QLabel(tr("Lingmo OS"));
    title->setObjectName("cardTitleLabel");
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 2);
    tf.setBold(true);
    title->setFont(tf);
    hl->addWidget(title);
    hl->addStretch();
    cardLayout->addWidget(header);

    // Pages: 0=Welcome, 1=Hostname, 2=Applying, 3=Finished
    m_stack = new QStackedWidget;
    m_stack->setObjectName("oobeStack");
    m_stack->addWidget(createWelcomePage());
    m_stack->addWidget(createHostnamePage());
    m_stack->addWidget(createApplyingPage());
    m_stack->addWidget(createFinishedPage());
    QScrollArea *scroll = new QScrollArea;
    scroll->setObjectName("oobeScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_stack);
    cardLayout->addWidget(scroll, 1);

    QWidget *nav = new QWidget;
    nav->setObjectName("cardNav");
    QHBoxLayout *nl = new QHBoxLayout(nav);
    nl->setContentsMargins(24, 12, 24, 20);

    m_backBtn = new QPushButton(tr("Back"));
    m_backBtn->setObjectName("navBackBtn");
    m_backBtn->setFixedSize(100, 36);
    connect(m_backBtn, &QPushButton::clicked, this, &OobeWindow::backPage);

    m_nextBtn = new QPushButton(tr("Next"));
    m_nextBtn->setObjectName("navNextBtn");
    m_nextBtn->setFixedSize(120, 36);
    connect(m_nextBtn, &QPushButton::clicked, this, &OobeWindow::nextPage);

    nl->addWidget(m_backBtn);
    nl->addStretch();
    nl->addWidget(m_nextBtn);
    cardLayout->addWidget(nav);

    updateNavButtons();

    m_cardWidget->setStyleSheet(
        "#oobeCard {"
        "  background: rgba(255, 255, 255, 0.93);"
        "  border-radius: 14px;"
        "}"
        "#cardHeader {"
        "  background: transparent;"
        "  border-bottom: 1px solid rgba(0, 0, 0, 0.08);"
        "}"
        "#cardTitleLabel {"
        "  color: #1a1a1a;"
        "  background: transparent;"
        "  margin-left: 8px;"
        "}"
        "#oobeStack {"
        "  background: transparent;"
        "}"
        "#cardNav {"
        "  background: transparent;"
        "  border-top: 1px solid rgba(0, 0, 0, 0.06);"
        "}"
        "#navBackBtn {"
        "  background: #F2F4F5;"
        "  border: none;"
        "  border-radius: 10px;"
        "  color: #323238;"
        "  font-size: 13px;"
        "}"
        "#navBackBtn:hover { background: #DCDEDF; }"
        "#navBackBtn:pressed { background: #C9CBCC; }"
        "#navNextBtn {"
        "  background: #3385FF;"
        "  border: none;"
        "  border-radius: 10px;"
        "  color: #FFFFFF;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "#navNextBtn:hover { background: #3892FF; }"
        "#navNextBtn:pressed { background: #2E79E7; }"
        "#navNextBtn:disabled { background: #A3C2F5; color: #FFFFFF; }"
    );

    return m_cardWidget;
}

QWidget* OobeWindow::createWelcomePage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageWelcome");

    QHBoxLayout *hl = new QHBoxLayout(page);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // Left brand panel
    QWidget *left = new QWidget;
    left->setObjectName("welcomeLeft");
    left->setMinimumWidth(LEFT_MIN_W);
    left->setMaximumWidth(LEFT_MAX_W);
    left->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout *ll = new QVBoxLayout(left);
    ll->setContentsMargins(40, 40, 40, 40);
    ll->addStretch();

    QLabel *bigLogo = new QLabel;
    QPixmap blpix(":/logo.png");
    if (!blpix.isNull())
        bigLogo->setPixmap(blpix.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    bigLogo->setAlignment(Qt::AlignCenter);
    ll->addWidget(bigLogo);
    ll->addSpacing(16);

    QLabel *brand = new QLabel("Lingmo OS");
    QFont bf = brand->font();
    bf.setPointSize(bf.pointSize() + 8);
    bf.setBold(true);
    brand->setFont(bf);
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet("color: #1a1a1a;");
    ll->addWidget(brand);

    const QString ver = osReleaseField("VERSION_ID");
    if (!ver.isEmpty()) {
        QLabel *verLabel = new QLabel(ver);
        verLabel->setAlignment(Qt::AlignCenter);
        verLabel->setStyleSheet("color: #64646E; font-size: 12px;");
        ll->addWidget(verLabel);
    }
    ll->addStretch();

    left->setStyleSheet("#welcomeLeft { background: rgba(240,242,245,0.7); border-radius: 0 0 0 14px; }");
    hl->addWidget(left);

    // Divider
    QFrame *div = new QFrame;
    div->setFrameShape(QFrame::VLine);
    div->setStyleSheet("color: rgba(0,0,0,0.08);");
    hl->addWidget(div);

    // Right content panel
    QWidget *right = new QWidget;
    QVBoxLayout *rl = new QVBoxLayout(right);
    rl->setContentsMargins(PAGE_PADDING, 40, PAGE_PADDING, 20);
    rl->setSpacing(16);
    rl->addStretch();

    QLabel *heading = new QLabel(tr("Welcome to Lingmo OS"));
    QFont hf = heading->font();
    hf.setPointSize(hf.pointSize() + 6);
    hf.setBold(true);
    heading->setFont(hf);
    heading->setWordWrap(true);
    rl->addWidget(heading);

    QLabel *sub = new QLabel(tr("Let's set up a few things before you get started. "
                                "This will only take a moment."));
    sub->setWordWrap(true);
    sub->setStyleSheet("color: #64646E; font-size: 14px;");
    rl->addWidget(sub);

    rl->addStretch();
    hl->addWidget(right, 1);

    // Hide left panel when card is too narrow
    connect(m_stack, &QStackedWidget::currentChanged, this, [left, div](int) {
        (void)left; (void)div;
    });

    return page;
}

QWidget* OobeWindow::createHostnamePage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageHostname");
    QVBoxLayout *l = new QVBoxLayout(page);
    l->setContentsMargins(PAGE_PADDING, 20, PAGE_PADDING, 10);
    l->setSpacing(16);

    QLabel *heading = new QLabel(tr("Computer Name"));
    QFont hf = heading->font();
    hf.setPointSize(hf.pointSize() + 4);
    hf.setBold(true);
    heading->setFont(hf);
    l->addWidget(heading);

    QLabel *sub = new QLabel(tr("Give your computer a name. It will appear on the network "
                                "and in file manager titles."));
    sub->setWordWrap(true);
    sub->setStyleSheet("color: #64646E; font-size: 13px;");
    l->addWidget(sub);
    l->addSpacing(8);

    const QString fieldStyle =
        "QLineEdit { border: 1px solid #D9DADD; border-radius: 10px; padding: 4px 10px; font-size: 14px; background: #FFFFFF; }"
        "QLineEdit:focus { border-color: #3385FF; }"
        "QLineEdit[invalid=\"true\"] { border-color: #D92D20; }";
    const QString labelStyle = "font-size: 12px; color: #64646E;";
    const QString errorStyle = "color: #D92D20; font-size: 12px; background: transparent;";

    auto *hostnameWidget = new QWidget;
    auto *hvl = new QVBoxLayout(hostnameWidget);
    hvl->setContentsMargins(0, 0, 0, 0); hvl->setSpacing(4);
    auto *hostnameLabel = new QLabel(tr("Computer name"));
    hostnameLabel->setStyleSheet(labelStyle);
    hvl->addWidget(hostnameLabel);
    m_hostnameEdit = new QLineEdit;
    m_hostnameEdit->setFixedHeight(36);
    m_hostnameEdit->setStyleSheet(fieldStyle);
    m_hostnameEdit->setMaxLength(63);
    m_hostnameEdit->setPlaceholderText(tr("e.g. lingmo-pc"));
    hvl->addWidget(m_hostnameEdit);
    m_hostnameError = new QLabel;
    m_hostnameError->setStyleSheet(errorStyle);
    m_hostnameError->setVisible(false);
    hvl->addWidget(m_hostnameError);
    l->addWidget(hostnameWidget);

    l->addStretch();
    return page;
}

QWidget* OobeWindow::createApplyingPage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageApplying");
    QVBoxLayout *l = new QVBoxLayout(page);
    l->setContentsMargins(PAGE_PADDING, 30, PAGE_PADDING, 30);
    l->setSpacing(24);

    QLabel *icon = new QLabel;
    QPixmap ipix(":/logo.png");
    if (!ipix.isNull())
        icon->setPixmap(ipix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    l->addWidget(icon);

    QLabel *heading = new QLabel(tr("Applying Settings"));
    QFont hf = heading->font();
    hf.setPointSize(hf.pointSize() + 4);
    hf.setBold(true);
    heading->setFont(hf);
    heading->setAlignment(Qt::AlignCenter);
    l->addWidget(heading);

    m_progressLabel = new QLabel(tr("Please wait..."));
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setStyleSheet("color: #666; font-size: 13px;");
    l->addWidget(m_progressLabel);

    m_progressBar = new QWidget;
    m_progressBar->setFixedHeight(4);
    m_progressBar->setStyleSheet("background: #e0e0e0; border-radius: 2px;");
    QHBoxLayout *pbl = new QHBoxLayout(m_progressBar);
    pbl->setContentsMargins(0, 0, 0, 0);

    QWidget *fill = new QWidget;
    fill->setObjectName("progressFill");
    fill->setFixedHeight(4);
    fill->setStyleSheet("background: #3385FF; border-radius: 2px;");
    fill->setFixedWidth(0);
    pbl->addWidget(fill);
    pbl->addStretch();

    l->addWidget(m_progressBar);
    l->addStretch();

    connect(m_stack, &QStackedWidget::currentChanged, this, [this, fill](int idx) {
        if (idx == 2) {
            m_backBtn->setEnabled(false);
            m_nextBtn->setEnabled(false);
            fill->setFixedWidth(0);

            auto *timer = new QTimer(this);
            timer->setInterval(60);
            int *w = new int(0);
            connect(timer, &QTimer::timeout, this, [this, timer, fill, w]() {
                *w += 4;
                const int maxW = m_progressBar->width();
                if (*w > maxW) *w = maxW;
                fill->setFixedWidth(*w);
                const double pct = maxW > 0 ? double(*w) / maxW : 0;
                if (pct < 0.35)
                    m_progressLabel->setText(tr("Setting computer name..."));
                else if (pct < 0.70)
                    m_progressLabel->setText(tr("Configuring auto login..."));
                else if (pct < 0.95)
                    m_progressLabel->setText(tr("Finishing..."));
                else {
                    timer->stop();
                    applySettings();
                }
            });
            timer->start();
        }
    });

    return page;
}

QWidget* OobeWindow::createFinishedPage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageFinished");
    QVBoxLayout *l = new QVBoxLayout(page);
    l->setContentsMargins(PAGE_PADDING, 30, PAGE_PADDING, 10);
    l->setSpacing(20);

    QLabel *icon = new QLabel;
    QPixmap ipix(":/logo.png");
    if (!ipix.isNull())
        icon->setPixmap(ipix.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    l->addWidget(icon);

    QLabel *heading = new QLabel(tr("Setup Complete"));
    QFont hf = heading->font();
    hf.setPointSize(hf.pointSize() + 6);
    hf.setBold(true);
    heading->setFont(hf);
    heading->setAlignment(Qt::AlignCenter);
    l->addWidget(heading);

    m_finishDesc = new QLabel;
    m_finishDesc->setObjectName("finishDesc");
    m_finishDesc->setAlignment(Qt::AlignCenter);
    QFont df = m_finishDesc->font();
    df.setPointSize(df.pointSize() + 1);
    m_finishDesc->setFont(df);
    m_finishDesc->setWordWrap(true);
    l->addWidget(m_finishDesc);

    connect(m_stack, &QStackedWidget::currentChanged, this, [this](int idx) {
        if (idx == m_stack->count() - 1)
            m_finishDesc->setText(tr("Welcome to Lingmo OS!\n\nYour system is ready. "
                                     "Click Reboot to start using it."));
    });

    l->addStretch();

    QPushButton *rebootBtn = new QPushButton(tr("Reboot Now"));
    rebootBtn->setObjectName("rebootBtn");
    rebootBtn->setFixedWidth(260);
    rebootBtn->setCursor(Qt::PointingHandCursor);
    l->addWidget(rebootBtn, 0, Qt::AlignCenter);

    connect(rebootBtn, &QPushButton::clicked, this, [rebootBtn]() {
        rebootBtn->setEnabled(false);
        rebootBtn->setText(tr("Rebooting..."));
        QCoreApplication::processEvents();
#ifdef Q_OS_UNIX
        QProcess::startDetached("systemctl", {"reboot"});
#endif
    });

    page->setStyleSheet(
        "#pageFinished { background: transparent; }"
        "#finishDesc { color: #64646E; }"
        "#rebootBtn {"
        "  background: #3385FF;"
        "  border: none;"
        "  border-radius: 10px;"
        "  color: #FFFFFF;"
        "  font-size: 15px;"
        "  font-weight: 600;"
        "  padding: 12px 0;"
        "}"
        "#rebootBtn:hover { background: #3892FF; }"
        "#rebootBtn:pressed { background: #2E79E7; }"
        "#rebootBtn:disabled { background: #A3C2F5; }"
    );

    return page;
}

void OobeWindow::updateNavButtons()
{
    const int idx  = m_stack->currentIndex();
    const int last = m_stack->count() - 1;

    // Back: visible on hostname page only (pages 1). Hidden on welcome(0),
    // applying(2) and finished(3).
    m_backBtn->setVisible(idx == 1);
    m_backBtn->setEnabled(idx == 1);

    if (idx == last) {
        m_nextBtn->setVisible(false);
        m_backBtn->setVisible(false);
    } else if (idx == last - 1) {
        // applying page — hide next; it auto-advances
        m_nextBtn->setVisible(false);
    } else {
        m_nextBtn->setVisible(true);
        m_nextBtn->setText(idx == 0 ? tr("Next") : tr("Next"));
    }
}

void OobeWindow::nextPage()
{
    const int idx = m_stack->currentIndex();

    if (idx == 1) {
        // Hostname page — validate
        m_hostname = m_hostnameEdit->text().trimmed().toLower();
        static const QRegularExpression kValidHost("^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$");
        if (m_hostname.isEmpty()) {
            m_hostname = "lingmo-pc";
        } else if (!kValidHost.match(m_hostname).hasMatch()) {
            m_hostnameError->setText(tr("Letters, digits and hyphens only. "
                                        "Cannot start or end with a hyphen."));
            m_hostnameError->setVisible(true);
            return;
        }
        m_hostnameError->setVisible(false);
    }

    fadeToPage(idx + 1);
}

void OobeWindow::backPage()
{
    const int idx = m_stack->currentIndex();
    if (idx > 0)
        fadeToPage(idx - 1);
}

void OobeWindow::fadeToPage(int index)
{
    if (index == m_stack->currentIndex())
        return;

    auto *effect = new QGraphicsOpacityEffect(m_stack);
    m_stack->setGraphicsEffect(effect);
    effect->setOpacity(1.0);

    auto *fadeOut = new QPropertyAnimation(effect, "opacity", this);
    fadeOut->setDuration(150);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this, index, effect]() {
        m_stack->setCurrentIndex(index);
        effect->setOpacity(0.0);

        auto *fadeIn = new QPropertyAnimation(effect, "opacity", this);
        fadeIn->setDuration(150);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);

        connect(fadeIn, &QPropertyAnimation::finished, this, [this]() {
            m_stack->setGraphicsEffect(nullptr);
            updateNavButtons();
        });

        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });

    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void OobeWindow::applySettings()
{
    m_progressLabel->setText(tr("Applying settings..."));
    QCoreApplication::processEvents();

    QString program = QStringLiteral("/usr/bin/python3");
    QStringList arguments{QStringLiteral("/usr/lib/lingmo-oobe/finish-setup.py"), m_hostname};
#ifdef Q_OS_UNIX
    if (getuid() != 0) {
        arguments.prepend(program);
        program = QStringLiteral("pkexec");
    }
#endif
    // Pass arguments directly: usernames and hostnames never become shell code.
    auto* process = new QProcess(this);
    m_backBtn->setEnabled(false);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int code, QProcess::ExitStatus status) {
        const bool ok = code == 0 && status == QProcess::NormalExit;
        process->deleteLater();
        m_backBtn->setEnabled(true);
        m_progressLabel->setText(ok ? tr("Setup complete") : tr("Setup failed"));
        if (ok) {
            QTimer::singleShot(500, this, [this] {
                m_stack->setCurrentIndex(m_stack->count() - 1);
                updateNavButtons();
            });
        } else {
            m_stack->setCurrentIndex(1);
            m_hostnameError->setText(tr("Setup failed"));
            updateNavButtons();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        process->deleteLater();m_backBtn->setEnabled(true);
        m_stack->setCurrentIndex(1);m_hostnameError->setText(tr("Setup failed"));updateNavButtons();
    });
    process->start(program, arguments);
}
