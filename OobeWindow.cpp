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
static const int CARD_MIN_W = 720;          // 再窄右侧表单就放不下
static const int CARD_MAX_W = 1100;         // 大屏不要把卡片拉得过宽
static const int CARD_MARGIN = 80;          // 卡片与屏幕边缘的余量
static const double CARD_MAX_H_RATIO = 0.86; // 卡片最高占屏幕高度的比例
static const int LEFT_MIN_W = 220;          // 左侧品牌面板宽度区间
static const int LEFT_MAX_W = 420;
static const int LEFT_HIDE_BELOW_W = 800;   // 卡片窄于此值时隐藏左面板
static const int PAGE_PADDING = 40;
static const char* OOBE_FLAG = "/etc/.firstboot-complete";

// 读取 os-release 字段（先 /usr/lib/os-release，再 /etc/os-release）。
// 版本号由此运行时获得，跟随系统版本，不再随源码写死。
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

    // Entrance fade-in
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

    // 宽度：按比例，但限制在 [minW, maxW]；minW 也要保证不超过屏宽-余量
    // 高度：不超过屏幕的 CARD_MAX_H_RATIO，超出部分由内部滚动区承担
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

    // --- Header ---
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

    // --- Stacked pages ---
    m_stack = new QStackedWidget;
    m_stack->setObjectName("oobeStack");
    m_stack->addWidget(createWelcomePage());
    m_stack->addWidget(createUserPage());
    m_stack->addWidget(createHostnamePage());
    m_stack->addWidget(createConfirmPage());
    m_stack->addWidget(createInstallPage());
    m_stack->addWidget(createFinishedPage());
    QScrollArea *scroll = new QScrollArea;
    scroll->setObjectName("oobeScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_stack);
    cardLayout->addWidget(scroll, 1);

    // --- Navigation ---
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

    // Left panel: logo / illustration
    QWidget *leftPanel = new QWidget;
    leftPanel->setObjectName("welcomeLeft");
    leftPanel->setFixedWidth(qBound(LEFT_MIN_W, m_cardWidth * 2 / 5, LEFT_MAX_W));
    leftPanel->setVisible(m_cardWidth >= LEFT_HIDE_BELOW_W);

    QVBoxLayout *ll = new QVBoxLayout(leftPanel);
    ll->setAlignment(Qt::AlignCenter);

    QLabel *icon = new QLabel(leftPanel);
    QPixmap ipix(":/logo.png");
    if (!ipix.isNull())
        icon->setPixmap(ipix.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    icon->setAlignment(Qt::AlignCenter);
    ll->addWidget(icon);

    QLabel *osName = new QLabel(tr("Lingmo OS"), leftPanel);
    osName->setAlignment(Qt::AlignCenter);
    QFont nf = osName->font();
    nf.setPointSize(nf.pointSize() + 4);
    nf.setBold(true);
    osName->setFont(nf);
    osName->setStyleSheet("color: white; background: transparent;");
    ll->addWidget(osName);

    QLabel *versionLabel = new QLabel(osReleaseField(QStringLiteral("VERSION_ID")), leftPanel);
    if (versionLabel->text().isEmpty())
        versionLabel->setVisible(false);
    versionLabel->setAlignment(Qt::AlignCenter);
    QFont vf = versionLabel->font();
    vf.setPointSize(vf.pointSize());
    versionLabel->setFont(vf);
    versionLabel->setStyleSheet("color: rgba(255,255,255,0.7); background: transparent;");
    ll->addWidget(versionLabel);

    // Right panel: welcome text
    QWidget *rightPanel = new QWidget;
    rightPanel->setObjectName("welcomeRight");
    rightPanel->setStyleSheet("#welcomeRight { background: transparent; }");
    QVBoxLayout *rl = new QVBoxLayout(rightPanel);
    rl->setContentsMargins(50, 40, 50, 40);
    rl->setSpacing(16);

    rl->addStretch();

    QLabel *welcome = new QLabel(tr("Welcome to Lingmo OS"));
    welcome->setObjectName("welcomeTitle");
    QFont wf = welcome->font();
    wf.setPointSize(wf.pointSize() + 10);
    wf.setBold(true);
    welcome->setFont(wf);
    welcome->setStyleSheet("color: #1a1a1a; background: transparent;");
    rl->addWidget(welcome);

    QLabel *desc = new QLabel(tr("Let's set up your system.\nClick \"Next\" to continue."));
    desc->setObjectName("welcomeDesc");
    QFont df = desc->font();
    df.setPointSize(df.pointSize() + 2);
    desc->setFont(df);
    desc->setStyleSheet("color: #666; background: transparent;");
    desc->setWordWrap(true);
    rl->addWidget(desc);

    rl->addStretch();

    hl->addWidget(leftPanel);
    hl->addWidget(rightPanel, 1);

    return page;
}

QWidget* OobeWindow::createUserPage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageUser");
    QVBoxLayout *l = new QVBoxLayout(page);
    l->setContentsMargins(PAGE_PADDING, 20, PAGE_PADDING, 10);
    l->setSpacing(16);

    QLabel *heading = new QLabel(tr("Create User"));
    QFont hf = heading->font();
    hf.setPointSize(hf.pointSize() + 4);
    hf.setBold(true);
    heading->setFont(hf);
    l->addWidget(heading);

    QLabel *sub = new QLabel(tr("Create a user account to log into the system."));
    l->addWidget(sub);
    l->addSpacing(8);

    const QString fieldStyle =
        "QLineEdit { border: 1px solid #D9DADD; border-radius: 10px; padding: 4px 10px; font-size: 14px; background: #FFFFFF; }"
        "QLineEdit:focus { border-color: #3385FF; }"
        "QLineEdit[invalid=\"true\"] { border-color: #D92D20; }";
    const QString labelStyle = "font-size: 12px; color: #64646E;";
    const QString errorStyle = "color: #D92D20; font-size: 12px; background: transparent;";

    // Username
    auto *usernameWidget = new QWidget;
    auto *uvl = new QVBoxLayout(usernameWidget);
    uvl->setContentsMargins(0, 0, 0, 0); uvl->setSpacing(4);
    auto *usernameLabel = new QLabel(tr("Username"));
    usernameLabel->setStyleSheet(labelStyle);
    uvl->addWidget(usernameLabel);
    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setFixedHeight(36);
    m_usernameEdit->setStyleSheet(fieldStyle);
    m_usernameEdit->setMaxLength(32);
    m_usernameEdit->setPlaceholderText(tr("e.g. alice"));
    // Only allow lowercase letters, digits, '-' and '_' while typing.
    m_usernameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[a-z_][a-z0-9_-]{0,31}"), m_usernameEdit));
    uvl->addWidget(m_usernameEdit);
    m_usernameError = new QLabel;
    m_usernameError->setStyleSheet(errorStyle);
    m_usernameError->setVisible(false);
    uvl->addWidget(m_usernameError);
    l->addWidget(usernameWidget);

    // Password
    auto *passwordWidget = new QWidget;
    auto *pvl = new QVBoxLayout(passwordWidget);
    pvl->setContentsMargins(0, 0, 0, 0); pvl->setSpacing(4);
    auto *passwordLabel = new QLabel(tr("Password"));
    passwordLabel->setStyleSheet(labelStyle);
    pvl->addWidget(passwordLabel);
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setFixedHeight(36);
    m_passwordEdit->setStyleSheet(fieldStyle);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    pvl->addWidget(m_passwordEdit);
    m_passwordError = new QLabel;
    m_passwordError->setStyleSheet(errorStyle);
    m_passwordError->setVisible(false);
    pvl->addWidget(m_passwordError);
    l->addWidget(passwordWidget);

    // Confirm password
    auto *confirmWidget = new QWidget;
    auto *cvl = new QVBoxLayout(confirmWidget);
    cvl->setContentsMargins(0, 0, 0, 0); cvl->setSpacing(4);
    auto *confirmLabel = new QLabel(tr("Confirm Password"));
    confirmLabel->setStyleSheet(labelStyle);
    cvl->addWidget(confirmLabel);
    m_confirmPwdEdit = new QLineEdit;
    m_confirmPwdEdit->setFixedHeight(36);
    m_confirmPwdEdit->setStyleSheet(fieldStyle);
    m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
    cvl->addWidget(m_confirmPwdEdit);
    m_confirmError = new QLabel;
    m_confirmError->setStyleSheet(errorStyle);
    m_confirmError->setVisible(false);
    cvl->addWidget(m_confirmError);
    l->addWidget(confirmWidget);

    l->addStretch();
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

    QLabel *sub = new QLabel(tr("Give this computer a name on the network."));
    l->addWidget(sub);
    l->addSpacing(8);

    auto *hostnameLabel = new QLabel(tr("Hostname"));
    hostnameLabel->setStyleSheet("font-size: 12px; color: #64646E;");
    l->addWidget(hostnameLabel);

    m_hostnameEdit = new QLineEdit(QStringLiteral("lingmo-pc"));
    m_hostnameEdit->setFixedHeight(36);
    m_hostnameEdit->setMaxLength(63);
    m_hostnameEdit->setPlaceholderText(tr("e.g. lingmo-pc"));
    m_hostnameEdit->setStyleSheet(
        "QLineEdit { border: 1px solid #D9DADD; border-radius: 10px; padding: 4px 10px; font-size: 14px; background: #FFFFFF; }"
        "QLineEdit:focus { border-color: #3385FF; }"
        "QLineEdit[invalid=\"true\"] { border-color: #D92D20; }");
    // RFC 1123: labels are [a-zA-Z0-9] and '-', not starting/ending with '-'.
    // Allow uppercase here; we lowercase at apply time.
    m_hostnameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("[a-zA-Z0-9][a-zA-Z0-9-]{0,62}"), m_hostnameEdit));
    l->addWidget(m_hostnameEdit);

    m_hostnameError = new QLabel;
    m_hostnameError->setStyleSheet("color: #D92D20; font-size: 12px; background: transparent;");
    m_hostnameError->setVisible(false);
    l->addWidget(m_hostnameError);

    QLabel *hint = new QLabel(tr("Letters, digits and hyphens only. Cannot start or end with a hyphen."));
    hint->setStyleSheet("font-size: 11px; color: #64646E; background: transparent;");
    hint->setWordWrap(true);
    l->addWidget(hint);

    l->addStretch();
    return page;
}

QWidget* OobeWindow::createConfirmPage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageConfirm");
    QVBoxLayout *l = new QVBoxLayout(page);
    l->setContentsMargins(PAGE_PADDING, 20, PAGE_PADDING, 10);
    l->setSpacing(16);

    QLabel *heading = new QLabel(tr("Confirm Settings"));
    QFont hf = heading->font();
    hf.setPointSize(hf.pointSize() + 4);
    hf.setBold(true);
    heading->setFont(hf);
    l->addWidget(heading);

    QLabel *info = new QLabel;
    info->setObjectName("confirmInfo");
    info->setStyleSheet("color: #333; font-size: 14px; line-height: 1.8; background: #f5f5f5; border-radius: 8px; padding: 20px;");
    info->setWordWrap(true);
    l->addWidget(info);

    l->addStretch();

    connect(m_stack, &QStackedWidget::currentChanged, this, [this, info](int idx) {
        if (idx == 3) {
            info->setText(
                tr("Username: %1\nComputer Name: %2")
                    .arg(m_username.isEmpty() ? tr("(not set)") : m_username)
                    .arg(m_hostname.isEmpty() ? tr("(not set)") : m_hostname));
        }
    });

    return page;
}

QWidget* OobeWindow::createInstallPage()
{
    QWidget *page = new QWidget;
    page->setObjectName("pageInstall");
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

    // Simple progress bar widget
    m_progressBar = new QWidget;
    m_progressBar->setFixedHeight(4);
    m_progressBar->setStyleSheet("background: #e0e0e0; border-radius: 2px;");
    QHBoxLayout *pbl = new QHBoxLayout(m_progressBar);
    pbl->setContentsMargins(0, 0, 0, 0);

    QWidget *fill = new QWidget;
    fill->setObjectName("progressFill");
    fill->setFixedHeight(4);
    fill->setStyleSheet("background: #0058a9; border-radius: 2px;");
    fill->setFixedWidth(0);
    pbl->addWidget(fill);

    l->addWidget(m_progressBar);

    l->addStretch();

    connect(m_stack, &QStackedWidget::currentChanged, this, [this, fill](int idx) {
        if (idx == 4) {
            m_backBtn->setEnabled(false);
            m_nextBtn->setEnabled(false);

            auto *timer = new QTimer(this);
            timer->setInterval(80);
            int *w = new int(0);
            connect(timer, &QTimer::timeout, this, [this, timer, fill, w]() {
                *w += 3;
                if (*w > m_progressBar->width())
                    *w = m_progressBar->width();
                fill->setFixedWidth(*w);
                if (*w < m_progressBar->width() * 0.15)
                    m_progressLabel->setText(tr("Creating user..."));
                else if (*w < m_progressBar->width() * 0.40)
                    m_progressLabel->setText(tr("Setting password..."));
                else if (*w < m_progressBar->width() * 0.65)
                    m_progressLabel->setText(tr("Setting hostname..."));
                else if (*w < m_progressBar->width() * 0.85)
                    m_progressLabel->setText(tr("Configuring auto login..."));
                else if (*w < m_progressBar->width() - 5)
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

    // m_username is set in nextPage() before this page is shown, so update
    // the label when we actually arrive here rather than at construction time.
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
            m_finishDesc->setText(
                tr("Welcome to Lingmo OS!\n\nYour system is ready.\nAfter reboot, log in with \"%1\".").arg(m_username));
    });

    l->addStretch();

    QPushButton *rebootBtn = new QPushButton(tr("Reboot Now"));
    rebootBtn->setObjectName("rebootBtn");
    rebootBtn->setFixedWidth(260);
    rebootBtn->setCursor(Qt::PointingHandCursor);
    l->addWidget(rebootBtn, 0, Qt::AlignCenter);

    // OOBE runs as root (applySettings executes privileged commands directly),
    // so systemctl reboot is always available without pkexec.
    connect(rebootBtn, &QPushButton::clicked, this, [rebootBtn]() {
        rebootBtn->setEnabled(false);
        rebootBtn->setText(tr("Rebooting..."));
        QCoreApplication::processEvents();
#ifdef Q_OS_UNIX
        QProcess::startDetached("systemctl", { "reboot" });
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
    int idx = m_stack->currentIndex();
    int last = m_stack->count() - 1;

    m_backBtn->setVisible(idx > 0 && idx < last - 1);

    if (idx == 0) {
        m_nextBtn->setText(tr("Next"));
    } else if (idx < last - 1) {
        m_nextBtn->setText(tr("Next"));
    } else if (idx == last - 1) {
        m_nextBtn->setText(tr("Finish"));
    } else {
        m_nextBtn->setVisible(false);
        m_backBtn->setVisible(false);
    }
}

void OobeWindow::nextPage()
{
    int idx = m_stack->currentIndex();

    if (idx == 1) {
        m_username = m_usernameEdit->text().trimmed();
        m_password = m_passwordEdit->text();
        const QString confirm = m_confirmPwdEdit->text();

        bool ok = true;

        // Username: non-empty, format already enforced by QValidator, but
        // re-check the full regex (validator allows intermediate states).
        static const QRegularExpression kValidName("^[a-z_][a-z0-9_-]{0,31}$");
        if (m_username.isEmpty()) {
            m_usernameError->setText(tr("Username must not be empty."));
            m_usernameError->setVisible(true);
            ok = false;
        } else if (!kValidName.match(m_username).hasMatch()) {
            m_usernameError->setText(tr("Must start with a letter or '_', then lowercase letters, digits, '-' or '_'. Max 32 characters."));
            m_usernameError->setVisible(true);
            ok = false;
        } else {
            // Reject system account names (UID < 1000).
            bool taken = false;
            QFile passwd("/etc/passwd");
            if (passwd.open(QIODevice::ReadOnly | QIODevice::Text)) {
                while (!passwd.atEnd()) {
                    const QString line = QString::fromUtf8(passwd.readLine());
                    const QStringList fields = line.split(':');
                    if (fields.size() >= 3 && fields.at(0) == m_username) {
                        bool uidOk = false;
                        const int uid = fields.at(2).toInt(&uidOk);
                        if (uidOk && uid < 1000) { taken = true; break; }
                    }
                }
            }
            if (taken) {
                m_usernameError->setText(tr("\"%1\" is a reserved system name. Please choose another.").arg(m_username));
                m_usernameError->setVisible(true);
                ok = false;
            } else {
                m_usernameError->setVisible(false);
            }
        }

        // Password: must not be empty.
        if (m_password.isEmpty()) {
            m_passwordError->setText(tr("Password must not be empty."));
            m_passwordError->setVisible(true);
            ok = false;
        } else {
            m_passwordError->setVisible(false);
        }

        // Confirm password: must match.
        if (!m_password.isEmpty() && m_password != confirm) {
            m_confirmError->setText(tr("Passwords do not match."));
            m_confirmError->setVisible(true);
            ok = false;
        } else {
            m_confirmError->setVisible(false);
        }

        if (!ok) return;
    }

    if (idx == 2) {
        m_hostname = m_hostnameEdit->text().trimmed().toLower();

        // RFC 1123: 1–63 chars, [a-z0-9-], must not start or end with '-'.
        static const QRegularExpression kValidHost("^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$");
        if (m_hostname.isEmpty()) {
            m_hostname = "lingmo-pc";
        } else if (!kValidHost.match(m_hostname).hasMatch()) {
            m_hostnameError->setText(tr("Letters, digits and hyphens only. Cannot start or end with a hyphen."));
            m_hostnameError->setVisible(true);
            return;
        }
        m_hostnameError->setVisible(false);
    }

    if (idx == m_stack->count() - 1) {
        QProcess::startDetached("systemctl", QStringList() << "reboot");
        qApp->quit();
        return;
    }

    fadeToPage(idx + 1);
}

void OobeWindow::backPage()
{
    int idx = m_stack->currentIndex();
    if (idx > 0) {
        fadeToPage(idx - 1);
    }
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

    QStringList cmds;
    // Remove the live user first so the chosen username is always available,
    // even when it matches the live account name (e.g. "lingmo"). The window
    // between deletion and creation is negligible; the OOBE-complete flag is
    // written only after useradd succeeds, so an interruption here will re-
    // enter OOBE on the next boot via the firstboot.service setup path, which
    // recreates an autologin account if none exists.
    cmds << "userdel -r live 2>/dev/null || true";
    cmds << QString("useradd -m -G sudo,audio,video,plugdev,netdev,bluetooth -s /bin/bash %1").arg(m_username);
    cmds << QString("echo '%1:%2' | chpasswd").arg(m_username, m_password);
    cmds << QString("hostnamectl set-hostname %1").arg(m_hostname);

    // The display manager is lingmo-dm: point its autologin at the account
    // the wizard created and at the regular desktop session.
    cmds << QString("printf '[Autologin]\\nUser=%1\\nSession=lingmo-wayland\\n' > /etc/lingmo-dm.conf").arg(m_username);

    // Mark OOBE complete
    cmds << QString("touch %1").arg(OOBE_FLAG);

    // Self-remove OOBE package to reduce footprint
    cmds << "apt-get remove -y lingmo-oobe 2>/dev/null || dpkg --purge lingmo-oobe 2>/dev/null || true";

    QString executor;
#ifdef Q_OS_UNIX
    if (getuid() != 0) {
        executor = "pkexec";
    }
#else
    executor = "pkexec";
#endif

    bool ok = true;
    for (const auto &cmd : cmds) {
        QString fullCmd;
        if (!executor.isEmpty())
            fullCmd = QString("%1 bash -c '%2'").arg(executor, cmd);
        else
            fullCmd = cmd;

        int ret = system(fullCmd.toUtf8().constData());
        if (ret != 0) {
            ok = false;
            break;
        }
        QCoreApplication::processEvents();
    }

    m_progressLabel->setText(ok ? tr("Setup complete") : tr("Setup failed"));

    QTimer::singleShot(500, this, [this]() {
        m_stack->setCurrentIndex(m_stack->count() - 1);
        m_nextBtn->setText(tr("Reboot"));
        m_nextBtn->setEnabled(true);
        updateNavButtons();
    });
}
