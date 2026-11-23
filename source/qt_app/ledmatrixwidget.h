#pragma once
#include <QWidget>
#include <QByteArray>

// Display the post-brightness RGB frame generated on the PC.
class LedMatrixWidget : public QWidget {
    Q_OBJECT
public:
    explicit LedMatrixWidget(QWidget *parent = nullptr);
    bool setFrame(const QByteArray &rgb);
    void clear();
    QSize sizeHint() const override { return {640, 220}; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QByteArray m_rgb;
};
