// vim:ts=4:sw=4:expandtab
#include "all.h"

static struct ev_loop *loop;
static struct ev_timer *idle_watcher;
static struct ev_io *x_watcher;
static struct ev_check *x_check;

static coordinates_t last_cursor_pos;

/* Forward declarations */
static void event_init_x_loop(void);
static void event_init_timer(void);
static void x_cb(EV_P_ ev_io *w, int revents);
static void x_check_cb(EV_P_ ev_check *w, int revents);
static void idle_cb(EV_P_ ev_timer *w, int revents);
static void event_select_xi(void);

void event_init(void) {
    event_select_xi();

    loop = EV_DEFAULT;
    event_init_x_loop();
    event_init_timer();
    ev_run(loop, 0);
}

static void event_init_x_loop(void) {
    x_watcher = calloc(sizeof(struct ev_io), 1);
    ev_io_init(x_watcher, x_cb, XConnectionNumber(display), EV_READ);
    ev_io_start(loop, x_watcher);

    x_check = calloc(sizeof(struct ev_check), 1);
    ev_check_init(x_check, x_check_cb);
    ev_check_start(loop, x_check);
}

static void event_init_timer(void) {
    idle_watcher = calloc(sizeof(struct ev_timer), 1);
    ev_timer_init(idle_watcher, idle_cb, 0, config.timeout);
    ev_timer_start(loop, idle_watcher);
}

static void x_cb(EV_P_ ev_io *w, int revents) {
    /* Deliberately empty. */
}

static void x_check_cb(EV_P_ ev_check *w, int revents) {
    XEvent ev;
    while (XPending(display) > 0) {
        XNextEvent(display, &ev);

        XGenericEventCookie *cookie = &ev.xcookie;
        if (cookie->type != GenericEvent || cookie->extension != xi_ext_opcode ||
                !XGetEventData(display, cookie)) {
            continue;
        }

        if (config.ignore_scrolling && cookie->evtype == XI_RawButtonPress) {
            const XIRawEvent *data = (const XIRawEvent *) cookie->data;
            if (data->detail == 4 || data->detail == 5) {
                XFreeEventData(display, cookie);
                continue;
            }
        }

        XFreeEventData(display, cookie);

        if (config.jitter > 0 && cookie->evtype == XI_RawMotion) {
            Window root, child;
            int root_x, root_y, win_x, win_y;
            unsigned int mask;

            XQueryPointer(display, DefaultRootWindow(display), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

            int dx = last_cursor_pos.x - root_x;
            int dy = last_cursor_pos.y - root_y;
            if (dx * dx + dy * dy < config.jitter * config.jitter) {
                continue;
            }

            last_cursor_pos.x = root_x;
            last_cursor_pos.y = root_y;
        }

        /* We don't bother checking the exact event since we only select events that interest us. */
        cursor_show();
        ev_timer_again(loop, idle_watcher);
    }
}

static void idle_cb(EV_P_ ev_timer *w, int revents) {
    if (!config.exclude_root || !cursor_on_root_window())
        cursor_hide();
}

static void event_select_xi(void) {
    XIEventMask masks[1];
    unsigned char mask[(XI_LASTEVENT + 7)/8];

    memset(mask, 0, sizeof(mask));
    XISetMask(mask, XI_RawMotion);
    XISetMask(mask, XI_RawButtonPress);

    masks[0].deviceid = XIAllMasterDevices;
    masks[0].mask_len = sizeof(mask);
    masks[0].mask = mask;

    XISelectEvents(display, DefaultRootWindow(display), masks, 1);
    XFlush(display);
}
