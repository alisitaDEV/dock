#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <set>
#include <cstdio>

/* ================= CONFIG ================= */
const int ICON_SIZE   = 40;
const int ICON_MARGIN = 4;
const int PADDING_X   = 5;
const int PADDING_Y   = 5;
const int BOTTOM_MARGIN = 10;

const double RADIUS = 10.0;
/* ========================================= */

struct App {
    std::string icon;
    std::string command;
    std::string wmclass;
};

/* ================= DRAW BACKGROUND ================= */
static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data) {
    GtkAllocation a;
    gtk_widget_get_allocation(widget, &a);

    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.5);

    cairo_new_path(cr);
    cairo_arc(cr, RADIUS, RADIUS, RADIUS, G_PI, 3 * G_PI / 2);
    cairo_arc(cr, a.width - RADIUS, RADIUS, RADIUS, 3 * G_PI / 2, 0);
    cairo_arc(cr, a.width - RADIUS, a.height - RADIUS, RADIUS, 0, G_PI / 2);
    cairo_arc(cr, RADIUS, a.height - RADIUS, RADIUS, G_PI / 2, G_PI);
    cairo_close_path(cr);

    cairo_fill(cr);
    return FALSE;
}

/* ================= DRAW INDICATOR ================= */
static gboolean draw_indicator(GtkWidget *widget, cairo_t *cr, gpointer data) {
    gboolean is_active = GPOINTER_TO_INT(data);

    GtkAllocation a;
    gtk_widget_get_allocation(widget, &a);

    if (is_active) {
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.8); // 🔥 active (hijau neon)
    } else {
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.7); // running (abu)
    }

    cairo_rectangle(cr, 0, 0, a.width, a.height);
    cairo_fill(cr);

    return FALSE;
}

/* ================= LOAD CONFIG ================= */
std::vector<App> load_apps(const std::string& filename) {
    std::vector<App> apps;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string icon, command, wmclass;

        if (std::getline(ss, icon, '|') &&
            std::getline(ss, command, '|') &&
            std::getline(ss, wmclass)) {

            apps.push_back({icon, command, wmclass});
        }
    }

    return apps;
}

/* ================= WMCTRL ================= */
std::set<std::string> get_running_apps() {
    std::set<std::string> result;

    FILE *fp = popen("wmctrl -lx", "r");
    if (!fp) return result;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        std::istringstream iss(line);
        std::string id, desktop, wmclass;
        iss >> id >> desktop >> wmclass;

        size_t dot = wmclass.rfind('.');
        if (dot != std::string::npos)
            wmclass = wmclass.substr(dot + 1);

        result.insert(wmclass);
    }

    pclose(fp);
    return result;
}

/* 🔥 ACTIVE WINDOW FIX (lebih stabil) */
std::string get_active_app() {
    FILE *fp = popen("xprop -root _NET_ACTIVE_WINDOW", "r");
    if (!fp) return "";

    char win_id[64];
    fscanf(fp, "%*s %*s %*s %*s %s", win_id);
    pclose(fp);

    std::string cmd = std::string("xprop -id ") + win_id + " WM_CLASS";
    fp = popen(cmd.c_str(), "r");

    if (!fp) return "";

    char line[512];
    std::string wmclass;

    if (fgets(line, sizeof(line), fp)) {
        std::string str(line);

        size_t last_quote = str.rfind('\"');
        size_t prev_quote = str.rfind('\"', last_quote - 1);

        if (prev_quote != std::string::npos && last_quote != std::string::npos) {
            wmclass = str.substr(prev_quote + 1, last_quote - prev_quote - 1);
        }
    }

    pclose(fp);
    return wmclass;
}

/* ================= LAUNCH ================= */
void launch_app(GtkWidget *widget, gpointer data) {
    const char *cmd = (const char*) data;
    std::string command = std::string(cmd) + " &";
    system(command.c_str());
}

/* ================= ICON ================= */
GtkWidget* create_icon_button(App app,
                             std::set<std::string> running,
                             std::string active) {

    GtkWidget *wrapper = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *button = gtk_button_new();

    GtkIconTheme *theme = gtk_icon_theme_get_default();
    GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(
        theme,
        app.icon.c_str(),
        ICON_SIZE,
        GTK_ICON_LOOKUP_FORCE_SIZE,
        NULL
    );

    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_container_add(GTK_CONTAINER(button), image);

    char *cmd = g_strdup(app.command.c_str());
    g_signal_connect(button, "clicked", G_CALLBACK(launch_app), cmd);

    gtk_box_pack_start(GTK_BOX(wrapper), button, FALSE, FALSE, 0);

    /* 🔥 MATCH */
    bool is_running = false;
    for (auto &r : running) {
        if (r.find(app.wmclass) != std::string::npos) {
            is_running = true;
            break;
        }
    }

    bool is_active = (active.find(app.wmclass) != std::string::npos);

    if (is_running) {
        GtkWidget *indicator = gtk_drawing_area_new();
        gtk_widget_set_size_request(indicator, ICON_SIZE, 3);

        g_signal_connect(indicator, "draw",
            G_CALLBACK(draw_indicator),
            GINT_TO_POINTER(is_active));

        gtk_box_pack_start(GTK_BOX(wrapper), indicator, FALSE, FALSE, 0);
    }

    return wrapper;
}

/* ================= MAIN ================= */
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_widget_set_app_paintable(window, TRUE);

    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);

    GdkVisual *visual = gdk_screen_get_rgba_visual(gdk_screen_get_default());
    if (visual) gtk_widget_set_visual(window, visual);

    const char* home = getenv("HOME");
    std::string config_path = std::string(home) + "/.config/mydock/apps.conf";

    std::vector<App> apps = load_apps(config_path);

    std::set<std::string> running = get_running_apps();
    std::string active = get_active_app();

    int icon_count = apps.size();

    int width = (ICON_SIZE * icon_count) +
                (ICON_MARGIN * 2 * icon_count) +
                (PADDING_X * 2);

    int height = ICON_SIZE + (PADDING_Y * 2);

    gtk_window_set_default_size(GTK_WINDOW(window), width, height);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(box), PADDING_Y);
    gtk_container_add(GTK_CONTAINER(window), box);

    for (auto &app : apps) {
        gtk_box_pack_start(GTK_BOX(box),
            create_icon_button(app, running, active),
            FALSE, FALSE, 0
        );
    }

    g_signal_connect(window, "draw", G_CALLBACK(draw_callback), NULL);

    gtk_widget_show_all(window);

    int rw, rh;
    gtk_window_get_size(GTK_WINDOW(window), &rw, &rh);

    GdkRectangle workarea;
    gdk_monitor_get_workarea(monitor, &workarea);

    int x = workarea.x + (workarea.width - rw) / 2;
    int y = workarea.y + workarea.height - rh - BOTTOM_MARGIN;

    gtk_window_move(GTK_WINDOW(window), x, y);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();
    return 0;
}
