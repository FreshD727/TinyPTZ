// Isolated test fixture, never included in the distributable plugin.
#include <QDockWidget>
#include <QMainWindow>
#include <obs-frontend-api.h>
#ifdef _WIN32
#define TINY_TEST_EXPORT __declspec(dllexport)
#else
#define TINY_TEST_EXPORT __attribute__((visibility("default")))
#endif
static QMainWindow *window;
static obs_source_t *first, *second, *current;
static obs_frontend_event_cb callback;
static void *callbackData;
extern "C" TINY_TEST_EXPORT void tiny_test_setup(QMainWindow *w, obs_source_t *a, obs_source_t *b) {
    window = w; first = a; second = b; current = a;
}
extern "C" TINY_TEST_EXPORT void tiny_test_scene(obs_source_t *s) { current = s; }
extern "C" TINY_TEST_EXPORT void tiny_test_event(obs_frontend_event event) {
    if (callback) callback(event, callbackData);
}
void *obs_frontend_get_main_window(void) { return window; }
void obs_frontend_get_scenes(obs_frontend_source_list *list) {
    da_push_back(list->sources, &first); obs_source_get_ref(first);
    da_push_back(list->sources, &second); obs_source_get_ref(second);
}
obs_source_t *obs_frontend_get_current_scene(void) { return obs_source_get_ref(current); }
char *obs_frontend_get_current_scene_collection(void) { return bstrdup("Isolated module test"); }
bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget) {
    if (window->findChild<QDockWidget *>(id)) return false;
    auto dock = new QDockWidget(title, window);
    dock->setObjectName(id); dock->setWidget(static_cast<QWidget *>(widget));
    window->addDockWidget(Qt::RightDockWidgetArea, dock);
    return true;
}
void obs_frontend_remove_dock(const char *id) {
    if (auto dock = window->findChild<QDockWidget *>(id)) delete dock;
}
void obs_frontend_add_event_callback(obs_frontend_event_cb cb, void *data) { callback = cb; callbackData = data; }
void obs_frontend_remove_event_callback(obs_frontend_event_cb cb, void *) { if (callback == cb) callback = nullptr; }
