#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QList>
#include <QSet>
#include <QPointer>
#include <QElapsedTimer>
#include <QPixmap>
#include <QImage>
#include <QSize>
#include <QEvent>

#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>

#include "agents.h"                // agents, DelayedMessage, MessageSource
#include "telegramaccountclient.h" // TDLib client for account

// Форвард-декларации для важких UI-класів
class QVBoxLayout;
class QHBoxLayout;
class QScrollArea;
class QStackedWidget;
class QTimer;
class QMovie;
class QPropertyAnimation;
class QVariantAnimation;
class QGraphicsOpacityEffect;
class QMouseEvent;
class QKeyEvent;
class QShowEvent;
class QPlainTextEdit;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Фон окна: живые цветные «блики», которые просвечивают сквозь стекло
// ---------------------------------------------------------------------------
class AnimatedBackdrop : public QWidget
{
    Q_OBJECT
public:
    explicit AnimatedBackdrop(QWidget *parent = nullptr);
    int contentMargin() const { return m_margin; }

    // Маленький (а значит размытый) кадр фона — его «берут» стеклянные панели
    const QImage &lowRes();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintContent(QPainter &painter);
    void paintBlobs(QPainter &painter);
    void paintAurora(QPainter &painter);
    void paintParticles(QPainter &painter);
    void paintStarfall(QPainter &painter);

    QString m_mode;
    QTimer *m_timer = nullptr;
    QMovie *m_movie = nullptr;
    QPixmap m_staticImage;
    qreal m_phase = 0.0;
    int m_margin = 0;

    QImage m_low;
    QSize m_lowForSize;
    QElapsedTimer m_lowClock;
};

// ---------------------------------------------------------------------------
// Стеклянная панель (имя сохранено ради совместимости). Рисует Liquid Glass:
// размытый фон, тинт, блик, кромку, тень, + опциональное акцентное свечение.
// ---------------------------------------------------------------------------
class NetworkBackdrop : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal glow READ glow WRITE setGlow)
public:
    enum class Surface { Panel, Card, Window, Pill };
    explicit NetworkBackdrop(Surface surface, QWidget *parent = nullptr);

    qreal glow() const { return m_glow; }
    void setGlow(qreal g) { m_glow = g; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintStarfall(QPainter &painter);
    void paintNodes(QPainter &painter);

    Surface m_surface;
    QString m_mode;
    bool m_enabled = false;
    QTimer *m_timer = nullptr;
    qreal m_phase = 0.0;
    qreal m_glow = 0.0;
};

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool checked, bool animate = true);

    qreal knobPos() const { return m_knobPos; }
    void setKnobPos(qreal pos);

signals:
    void toggled(bool checked);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_checked = false;
    qreal m_knobPos = 0.0;
    QPropertyAnimation *m_anim = nullptr;
};

class AnimatedIconButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal spin READ spin WRITE setSpin)
public:
    AnimatedIconButton(const QString &glyph, const QString &baseColor,
                       const QString &hoverColor, QWidget *parent = nullptr);

    qreal spin() const { return m_spin; }
    void setSpin(qreal degrees);

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_glyph;
    QString m_baseColor;
    QString m_hoverColor;
    QPropertyAnimation *m_anim = nullptr;
    qreal m_spin = 0.0;
};

class AgentAvatar : public QWidget
{
    Q_OBJECT
public:
    explicit AgentAvatar(QWidget *parent = nullptr);
    void setAgentName(const QString &name);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_initials;
    int m_paletteIndex = 0;
};

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent = nullptr);
    explicit ClickableLabel(const QString &text, QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
};

class ClickableFrame : public QFrame
{
    Q_OBJECT
public:
    explicit ClickableFrame(QWidget *parent = nullptr);
    void setSelected(bool selected);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    void applyStyle();
    bool m_selected = false;
    bool m_hovered = false;
};

class SubagentCard : public QFrame
{
    Q_OBJECT
public:
    enum class Status { Idle, Active, Busy, Error };

    SubagentCard(const QString &id, const QString &name, QWidget *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const;
    void setName(const QString &name);

    QString role() const { return m_role; }
    void setRole(const QString &role);

    void setStatus(Status status);
    void setAgentEnabled(bool enabled);
    void setCollapsed(bool collapsed);
    void setSelected(bool selected);

signals:
    void clicked(const QString &id);
    void nameEdited(const QString &id, const QString &newName);
    void enabledToggled(const QString &id, bool enabled);
    void settingsRequested(const QString &id);

protected:
    void mousePressEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEnterEvent *event) override;
#else
    void enterEvent(QEvent *event) override;
#endif
    void leaveEvent(QEvent *event) override;

private:
    void applyCardStyle();
    void applyStatusStyle();
    void refreshRolePreview();

    QString m_id;
    QString m_role;
    Status m_status = Status::Idle;
    bool m_enabled = true;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_collapsed = false;

    QVBoxLayout *m_outer = nullptr;
    AgentAvatar *m_avatar = nullptr;
    QLabel *m_statusDot = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    ToggleSwitch *m_toggleSwitch = nullptr;
    AnimatedIconButton *m_settingsButton = nullptr;
    ClickableLabel *m_roleLabel = nullptr;
    ClickableLabel *m_statusLabel = nullptr;
};

// ===========================================================================
// MainWindow
// ===========================================================================
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(agents *agentManager, QWidget *parent = nullptr);
    ~MainWindow() override;

    enum class AgentStatus { Idle, Active, Busy, Error };

    void appendMainMessage(const QString &text, bool isUser, const QString &code = QString());
    void appendSubagentMessage(const QString &id, const QString &text, bool isUser, const QString &code = QString());

    void setAgentActive(bool active);
    void setMainAgentName(const QString &name);
    void setMainAgentRole(const QString &role);

    void setSubagentStatus(const QString &id, AgentStatus status);
    void setSubagentName(const QString &id, const QString &name);
    void setSubagentRole(const QString &id, const QString &role);

    QString subagentName(const QString &id) const;
    QString subagentRole(const QString &id) const;
    bool hasSubagent(const QString &id) const;
    QStringList subagentIds() const;

    QString addSubagent(const QString &name = QString(), const QString &role = QString());
    void removeSubagent(const QString &id);

signals:
    void mainAgentSelected();
    void subagentSelected(const QString &id);
    void addSubagentRequested();
    void subagentRemoveRequested(const QString &id);
    void mainAgentRenamed(const QString &newName);
    void subagentRenamed(const QString &id, const QString &newName);
    void mainAgentRoleChanged(const QString &role);
    void subagentRoleChanged(const QString &id, const QString &role);
    void messageSubmitted(const QString &agentId, const QString &backendName, const QString &text);
    void appTelegramTokenChanged(const QString &token);
    void appAiKeyChanged(const QString &apiKey);
    void appAiModelChanged(const QString &model);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void handleAgentReplyForUI(qint64 chatId, const QList<DelayedMessage> &messages);
    void handleThinkingContext(const QString &stage, const QString &message);

    void handleSendClicked();
    void updateUptime();

    void showSettings();
    void showMainView();

    void selectMainAgent();
    void selectSubagent(const QString &id);

    void refreshAgentsFromDisk();
    void toggleAgentListCollapsed();

    void openBackgroundPicker();
    void openThemeCreator(const QString &themeId);
    void openAppSettings();
    void openMainAgentSettings();
    void openSubagentSettings(const QString &id);

private:
    struct ChatEntry {
        QString text;
        bool isUser;
        QString time;
        QString code; // код, если сообщение его содержит (сворачиваемый блок)
    };

    enum class AgentSettingsResult { Saved, Cancelled, DeleteRequested };

    // --- persistence / backend ---
    void loadAgentsFromDisk();
    void syncAgentName(const QString &backendName, const QString &newName);

    // --- ui construction ---
    void buildUi();
    QWidget *buildMainView();
    QWidget *buildSidebar();
    QWidget *buildChatArea();
    QWidget *buildSettingsPage();
    QWidget *buildCodePanel();
    SubagentCard *createCardWidget(const QString &id, const QString &name);

    void switchTheme(const QString &themeName);
    void rebuildUiLive();
    void performUiRebuild();
    void applyCollapsedVisualState();
    void fadeToPage(int index);

    // --- chat / history ---
    void appendHistory(const QString &agentId, const QString &text, bool isUser, const QString &code = QString());
    void renderMessage(const ChatEntry &entry, bool animate = false);
    void redrawActiveHistory();
    void refreshComposerPlaceholder();
    void receiveAgentReply(const QString &agentId, const QString &text, const QString &code = QString());
    qint64 uiChatIdFor(const QString &agentId);
    void scrollChatToBottom(bool smooth);
    void updateTypingIndicator();

    // --- code panel ---
    void showCodePanel(const QString &code, const QString &title = QString());
    void hideCodePanel();
    void advanceCodeTypewriter();
    void updateTypewriterCursorDisplay();

    // --- dialogs ---
    AgentSettingsResult runAgentSettingsDialog(const QString &title, const QString &backendAgentName,
                                               QString &name, QString &role, bool allowDelete);

    // --- constants ---
    static constexpr int kSidebarExpandedWidth = 260;
    static constexpr int kSidebarCollapsedWidth = 64;
    static constexpr int kCodePanelWidth = 440;

    Ui::MainWindow *ui = nullptr;
    agents *m_agentManager = nullptr;
    TelegramAccountClient *m_telegramAccountClient = nullptr;

    QString m_mainAgentName;
    QString m_mainAgentRole;
    QString m_activeAgentId;
    int m_subagentSeq = 0;
    bool m_agentListCollapsed = false;
    bool m_introPlayed = false;

    QMap<QString, SubagentCard *> m_cards;
    QMap<QString, QList<ChatEntry>> m_histories;
    QMap<QString, QString> m_agentFolderNames; // id -> backend folder name ("" == главный агент)
    QMap<QString, bool> m_agentEnabled;
    QMap<QString, qint64> m_uiChatIds;         // id -> синтетический (отрицательный) chatId
    QSet<QString> m_pending;                   // агенты, от которых ждём ответ (индикатор «печатает»)

    QElapsedTimer startTime;
    QTimer *uptimeTimer = nullptr;

    // --- top-level layout ---
    QStackedWidget *pageStack = nullptr;

    // --- sidebar ---
    QWidget *sidebarPanel = nullptr;
    ClickableFrame *mainStatusCard = nullptr;
    AgentAvatar *mainAvatar = nullptr;
    QLabel *agentStatusDot = nullptr;
    QLineEdit *mainNameEdit = nullptr;
    AnimatedIconButton *mainSettingsButton = nullptr;
    ClickableLabel *mainRoleLabel = nullptr;
    ClickableLabel *uptimeLabel = nullptr;
    QLabel *sidebarTitleLabel = nullptr;
    QLabel *subagentCountLabel = nullptr;
    AnimatedIconButton *refreshAgentsButton = nullptr;
    QPushButton *addSubagentButton = nullptr;
    AnimatedIconButton *collapseListButton = nullptr;
    QScrollArea *subagentsScroll = nullptr;
    QVBoxLayout *subagentsLayout = nullptr;
    AnimatedIconButton *settingsEntryButton = nullptr;

    // --- code panel ---
    QTimer *codeTypewriterTimer = nullptr;
    QTimer *codeTypewriterCursorBlink = nullptr;
    QString m_typewriterFullText;
    int m_typewriterPos = 0;
    bool m_typewriterCursorVisible = false;
    bool m_typewriterCursorDrawn = false;
    QFrame *codePanelSeparator = nullptr;
    QWidget *codePanelContainer = nullptr;
    QLabel *codePanelTitle = nullptr;
    QPlainTextEdit *codePanelBody = nullptr;
    bool m_codePanelOpen = false;

    // --- chat area ---
    AgentAvatar *chatHeaderAvatar = nullptr;
    QLabel *chatHeaderLabel = nullptr;
    QLabel *chatHeaderSub = nullptr;
    QScrollArea *chatScroll = nullptr;
    QWidget *chatBubblesHost = nullptr;
    QVBoxLayout *chatBubblesLayout = nullptr;
    QWidget *typingRow = nullptr;
    QPointer<QPropertyAnimation> m_scrollAnim;
    bool m_stickBottom = true;
    bool m_autoScrolling = false;

    QFrame *thinkingChip = nullptr;
    QLabel *thinkingDot = nullptr;
    QLabel *thinkingText = nullptr;
    QGraphicsOpacityEffect *thinkingOpacity = nullptr;
    QPropertyAnimation *thinkingFade = nullptr;
    QVariantAnimation *thinkingPulse = nullptr;
    QTimer *thinkingHideTimer = nullptr;

    NetworkBackdrop *inputGlass = nullptr;
    QPropertyAnimation *inputGlowAnim = nullptr;
    QPlainTextEdit *messageInput = nullptr;
    QPushButton *sendButton = nullptr;
};

#endif // MAINWINDOW_H