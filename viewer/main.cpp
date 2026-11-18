#include <gtk/gtk.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>
#include <array>

namespace fs = std::filesystem;

struct Viewer {
    GtkWidget *window{}, *canvas{}, *status{}, *play_button{}, *fps{};
    std::vector<fs::path> files;
    std::array<std::vector<fs::path>, 6> cameras;
    std::array<GdkPixbuf *, 6> grid{};
    int camera = -1;
    GdkPixbuf *image{};
    size_t index = 0;
    bool demo = false, snapshots = false, playing = false, fullscreen = false;
    guint timer = 0;

    size_t count() const {
        if (camera >= 0) {
            if (camera < 6) return cameras[camera].size();
            size_t n = cameras[0].size();
            for (const auto &c : cameras) n = std::min(n, c.size());
            return n;
        }
        return demo ? 120 : files.size();
    }
    void stop() {
        playing = false;
        if (timer) { g_source_remove(timer); timer = 0; }
        gtk_button_set_label(GTK_BUTTON(play_button), "Play  [Space]");
    }
    void message(const std::string &text) { gtk_label_set_text(GTK_LABEL(status), text.c_str()); }
    void show() {
        if (!count()) return;
        if (camera >= 0) {
            for (auto &p : grid) { if (p) g_object_unref(p); p = nullptr; }
            for (int i = 0; i < 6; ++i) {
                if (camera != 6 && camera != i) continue;
                GError *error = nullptr;
                grid[i] = gdk_pixbuf_new_from_file(cameras[i][index].c_str(), &error);
                if (!grid[i]) {
                    stop(); message(std::string("Could not load camera frame: ") + (error ? error->message : "unknown error"));
                    if (error) g_error_free(error);
                    return;
                }
            }
            message(std::string(camera == 6 ? "All six streams (frame-number aligned, not timestamp synchronized)" : "Camera " + std::to_string(camera + 1)) +
                    "    |    Frame " + std::to_string(index + 1) + " / " + std::to_string(count()));
            gtk_widget_queue_draw(canvas);
            return;
        }
        GdkPixbuf *next = nullptr;
        GError *error = nullptr;
        if (demo) {
            next = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 640, 400);
            auto *pixels = gdk_pixbuf_get_pixels(next);
            int stride = gdk_pixbuf_get_rowstride(next);
            double t = index * 6.283185307179586 / 120.0;
            int cx = 320 + static_cast<int>(210 * std::cos(t));
            int cy = 200 + static_cast<int>(125 * std::sin(t));
            for (int y = 0; y < 400; ++y) for (int x = 0; x < 640; ++x) {
                auto *p = pixels + y * stride + x * 3;
                int dx = x - cx, dy = y - cy;
                bool dot = dx * dx + dy * dy < 20 * 20;
                bool grid = x % 80 == 0 || y % 80 == 0;
                p[0] = dot ? 255 : (grid ? 48 : 12);
                p[1] = dot ? 220 : (grid ? 70 : 20);
                p[2] = dot ? 40 : (grid ? 90 : 35);
            }
        } else {
            next = gdk_pixbuf_new_from_file(files[index].string().c_str(), &error);
        }
        if (!next) {
            stop();
            message("Could not open frame: " + files[index].filename().string() + " — " +
                    (error ? error->message : "unknown error"));
            if (error) g_error_free(error);
            return;
        }
        if (image) g_object_unref(image);
        image = next;
        std::string name = demo ? "SYNTHETIC DEMO — not camera data" : files[index].parent_path().filename().string() + " / " + files[index].filename().string();
        if (snapshots) name = "SNAPSHOT STILL — " + name;
        message(name + "    |    " + (snapshots ? "Image " : "Frame ") + std::to_string(index + 1) + " / " + std::to_string(count()));
        gtk_widget_queue_draw(canvas);
    }
    void step(int delta) {
        if (!count()) return;
        index = static_cast<size_t>((static_cast<long long>(index) + delta + static_cast<long long>(count())) % static_cast<long long>(count()));
        show();
    }
    void start() {
        if (snapshots) { message("Snapshot stills are not a video. Use Previous or Next to browse them."); return; }
        if (!count() || playing) return;
        playing = true;
        gtk_button_set_label(GTK_BUTTON(play_button), "Pause  [Space]");
        unsigned interval = static_cast<unsigned>(std::max(1.0, 1000.0 / gtk_spin_button_get_value(GTK_SPIN_BUTTON(fps))));
        timer = g_timeout_add(interval, [](gpointer data) -> gboolean {
            auto *v = static_cast<Viewer *>(data);
            // Clear the ID before stepping: a read error can stop playback inside show().
            v->timer = 0;
            v->step(1);
            if (!v->playing) return G_SOURCE_REMOVE;
            // Keep the current source ID available to stop() between ticks.
            v->timer = g_source_get_id(g_main_current_source());
            return G_SOURCE_CONTINUE;
        }, this);
    }
    void toggle() { if (playing) stop(); else start(); }
    void load_demo() { stop(); camera = -1; files.clear(); demo = true; snapshots = false; index = 0; show(); }
    bool load_folder(const char *folder, bool snapshot_mode = false) {
        std::vector<fs::path> found;
        std::error_code ec;
        auto collect = [&](const fs::path &dir) {
            fs::directory_iterator it(dir, ec), end;
            if (ec) return;
            for (; it != end; it.increment(ec)) {
                if (ec) break;
                std::error_code file_error;
                if (!it->is_regular_file(file_error)) continue;
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(g_ascii_tolower(c)); });
                if (ext == ".png") found.push_back(it->path());
            }
        };
        if (snapshot_mode) {
            for (const char *name : {"BeginningSnapshot", "EndSnapshot"}) {
                std::error_code exists_error;
                fs::path dir = fs::path(folder) / name;
                if (fs::is_directory(dir, exists_error)) collect(dir);
                if (ec) break;
            }
        } else {
            collect(folder);
        }
        if (ec || found.empty()) {
            stop();
            message(ec ? "Cannot read folder: " + ec.message() : "No PNG frames found in that folder.");
            return false;
        }
        std::sort(found.begin(), found.end());
        stop(); camera = -1; files = std::move(found); demo = false; snapshots = snapshot_mode; index = 0; show();
        return true;
    }
    bool load_cameras(const char *root) {
        for (int i = 0; i < 6; ++i) {
            std::error_code ec;
            fs::path dir = fs::path(root) / ("camera-" + std::to_string(i + 1));
            for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
                if (it->path().extension() == ".png") cameras[i].push_back(it->path());
            }
            std::sort(cameras[i].begin(), cameras[i].end());
            if (ec || cameras[i].empty()) { message("Could not load camera " + std::to_string(i + 1)); return false; }
        }
        select_camera(0);
        return true;
    }
    void select_camera(int selected) {
        if (cameras[0].empty()) { message("Camera recording is not loaded."); return; }
        bool was_playing = playing; stop(); camera = selected; demo = snapshots = false;
        if (index >= count()) index = 0;
        show(); if (was_playing) start();
    }
    void choose_folder() {
        stop();
        GtkWidget *dialog = gtk_file_chooser_dialog_new("Open decoded frames folder", GTK_WINDOW(window),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, nullptr);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char *folder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (folder) { load_folder(folder); g_free(folder); }
        }
        gtk_widget_destroy(dialog);
    }
};

static GtkWidget *button(GtkWidget *box, const char *label, GCallback cb, Viewer *v) {
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(box), b, FALSE, FALSE, 0);
    g_signal_connect(b, "clicked", cb, v);
    return b;
}

int main(int argc, char **argv) {
    if (argc > 3 || (argc == 3 && std::string(argv[1]) != "--snapshots" && std::string(argv[1]) != "--cameras") || (argc == 2 && std::string(argv[1]) == "--help")) {
        g_print("Usage: tracking-viewer [decoded-PNG-folder | --snapshots snapshot-parent-folder]\nSee viewer/README.md for controls and interchange.\n");
        return argc > 2 ? 1 : 0;
    }
    if (!gtk_init_check(&argc, &argv)) { g_printerr("Cannot open a graphical display.\n"); return 1; }
    Viewer v;
    v.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(v.window), "Tracking Frame Viewer");
    gtk_window_set_default_size(GTK_WINDOW(v.window), 1100, 760);
    g_signal_connect(v.window, "destroy", G_CALLBACK(+[](GtkWidget *, gpointer data) {
        auto *v = static_cast<Viewer *>(data); v->stop(); gtk_main_quit();
    }), &v);
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, "* { font-size: 18px; } button { padding: 10px; }", -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 10);
    gtk_container_add(GTK_CONTAINER(v.window), outer);
    GtkWidget *help = gtk_label_new("Open folder: decoded PNGs  •  Demo: synthetic frames\nSpace: play/pause  •  ←/→: step  •  Home/End: first/last  •  F: fullscreen  •  Q: quit");
    gtk_label_set_line_wrap(GTK_LABEL(help), TRUE);
    gtk_box_pack_start(GTK_BOX(outer), help, FALSE, FALSE, 0);
    GtkWidget *camera_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(outer), camera_bar, FALSE, FALSE, 0);
    for (int i = 0; i < 7; ++i) {
        const char *labels[] = {"1 Scene R", "2 Scene L", "3 LEDs R", "4 LEDs L", "5 Upper L", "6 Upper R", "All 6"};
        GtkWidget *b = gtk_button_new_with_label(labels[i]);
        gtk_box_pack_start(GTK_BOX(camera_bar), b, TRUE, TRUE, 0);
        g_object_set_data(G_OBJECT(b), "camera-index", GINT_TO_POINTER(i));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton *b, gpointer p) {
            static_cast<Viewer *>(p)->select_camera(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "camera-index")));
        }), &v);
    }
    GtkWidget *scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_box_pack_start(GTK_BOX(outer), scroll, FALSE, FALSE, 0);
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_add(GTK_CONTAINER(scroll), bar);
    button(bar, "Open folder  [O]", G_CALLBACK(+[](GtkButton *, gpointer p) { static_cast<Viewer *>(p)->choose_folder(); }), &v);
    button(bar, "Demo  [D]", G_CALLBACK(+[](GtkButton *, gpointer p) { static_cast<Viewer *>(p)->load_demo(); }), &v);
    button(bar, "◀ Previous", G_CALLBACK(+[](GtkButton *, gpointer p) { auto *v = static_cast<Viewer *>(p); v->stop(); v->step(-1); }), &v);
    v.play_button = button(bar, "Play  [Space]", G_CALLBACK(+[](GtkButton *, gpointer p) { static_cast<Viewer *>(p)->toggle(); }), &v);
    button(bar, "Next ▶", G_CALLBACK(+[](GtkButton *, gpointer p) { auto *v = static_cast<Viewer *>(p); v->stop(); v->step(1); }), &v);
    gtk_box_pack_start(GTK_BOX(bar), gtk_label_new("FPS:"), FALSE, FALSE, 0);
    v.fps = gtk_spin_button_new_with_range(1, 120, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(v.fps), 30);
    gtk_box_pack_start(GTK_BOX(bar), v.fps, FALSE, FALSE, 0);
    g_signal_connect(v.fps, "value-changed", G_CALLBACK(+[](GtkSpinButton *, gpointer p) {
        auto *v = static_cast<Viewer *>(p); if (v->playing) { v->stop(); v->start(); }
    }), &v);
    v.status = gtk_label_new("Choose Demo or open a folder of decoded PNG frames.");
    gtk_label_set_line_wrap(GTK_LABEL(v.status), TRUE);
    gtk_box_pack_start(GTK_BOX(outer), v.status, FALSE, FALSE, 0);
    v.canvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(v.canvas, 320, 200);
    gtk_box_pack_start(GTK_BOX(outer), v.canvas, TRUE, TRUE, 0);
    g_signal_connect(v.canvas, "draw", G_CALLBACK(+[](GtkWidget *widget, cairo_t *cr, gpointer p) -> gboolean {
        auto *v = static_cast<Viewer *>(p);
        int w = gtk_widget_get_allocated_width(widget); int h = gtk_widget_get_allocated_height(widget);
        cairo_set_source_rgb(cr, 0.03, 0.03, 0.04); cairo_paint(cr);
        if (v->camera >= 0) {
            for (int i = 0; i < 6; ++i) {
                auto *im = v->grid[i]; if (!im) continue;
                double cw = v->camera == 6 ? w / 3.0 : w; double ch = v->camera == 6 ? h / 2.0 : h;
                double x = v->camera == 6 ? (i % 3) * cw : 0; double y = v->camera == 6 ? (i / 3) * ch : 0;
                int iw = gdk_pixbuf_get_width(im); int ih = gdk_pixbuf_get_height(im);
                double scale = std::min(cw / iw, ch / ih);
                cairo_save(cr); cairo_translate(cr, x + (cw - iw * scale) / 2, y + (ch - ih * scale) / 2);
                cairo_scale(cr, scale, scale); gdk_cairo_set_source_pixbuf(cr, im, 0, 0); cairo_paint(cr); cairo_restore(cr);
                if (v->camera == 6) { cairo_set_source_rgb(cr, 1, 1, 1); cairo_set_font_size(cr, 18); cairo_move_to(cr, x + 8, y + 22); std::string label = "Camera " + std::to_string(i + 1); cairo_show_text(cr, label.c_str()); }
            }
            return FALSE;
        }
        if (!v->image) return FALSE;
        int iw = gdk_pixbuf_get_width(v->image); int ih = gdk_pixbuf_get_height(v->image);
        double scale = std::min(static_cast<double>(w) / iw, static_cast<double>(h) / ih);
        cairo_translate(cr, (w - iw * scale) / 2, (h - ih * scale) / 2);
        cairo_scale(cr, scale, scale); gdk_cairo_set_source_pixbuf(cr, v->image, 0, 0); cairo_paint(cr);
        return FALSE;
    }), &v);
    g_signal_connect(v.window, "key-press-event", G_CALLBACK(+[](GtkWidget *, GdkEventKey *e, gpointer p) -> gboolean {
        auto *v = static_cast<Viewer *>(p);
        // Let the FPS field receive normal editing keys.
        if (gtk_widget_has_focus(v->fps) && e->keyval != GDK_KEY_Escape) return FALSE;
        switch (e->keyval) {
        case GDK_KEY_space: v->toggle(); break;
        case GDK_KEY_Left: v->stop(); v->step(-1); break;
        case GDK_KEY_Right: v->stop(); v->step(1); break;
        case GDK_KEY_Home: v->stop(); v->index = 0; v->show(); break;
        case GDK_KEY_End: v->stop(); if (v->count()) { v->index = v->count() - 1; v->show(); } break;
        case GDK_KEY_o: case GDK_KEY_O: v->choose_folder(); break;
        case GDK_KEY_d: case GDK_KEY_D: v->load_demo(); break;
        case GDK_KEY_f: case GDK_KEY_F:
            v->fullscreen = !v->fullscreen;
            if (v->fullscreen) gtk_window_fullscreen(GTK_WINDOW(v->window)); else gtk_window_unfullscreen(GTK_WINDOW(v->window));
            break;
        case GDK_KEY_Escape: v->fullscreen = false; gtk_window_unfullscreen(GTK_WINDOW(v->window)); break;
        case GDK_KEY_q: case GDK_KEY_Q: gtk_widget_destroy(v->window); break;
        default: return FALSE;
        }
        return TRUE;
    }), &v);
    if (argc == 2) v.load_folder(argv[1]);
    if (argc == 3 && std::string(argv[1]) == "--snapshots") v.load_folder(argv[2], true);
    if (argc == 3 && std::string(argv[1]) == "--cameras") v.load_cameras(argv[2]);
    gtk_widget_show_all(v.window);
    gtk_main();
    if (v.image) g_object_unref(v.image);
    for (auto *p : v.grid) if (p) g_object_unref(p);
    return 0;
}
