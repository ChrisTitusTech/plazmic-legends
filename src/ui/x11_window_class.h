#pragma once

#include <QObject>

class QApplication;
class QEvent;
class QWidget;

namespace plazmic {

inline constexpr const char* kX11Instance = "plazmic-legends";
inline constexpr const char* kX11Class = "PlazmicLegends";

class X11WindowClassFilter final : public QObject {
  public:
    explicit X11WindowClassFilter(QObject* parent = nullptr);
    ~X11WindowClassFilter() override;

    X11WindowClassFilter(const X11WindowClassFilter&) = delete;
    X11WindowClassFilter& operator=(const X11WindowClassFilter&) = delete;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool apply(QWidget* widget) const;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    struct State;
    State* state_;
};

[[nodiscard]] X11WindowClassFilter* install_x11_window_class_filter(
    QApplication& application);

}  // namespace plazmic
