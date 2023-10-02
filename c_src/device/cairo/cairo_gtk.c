#include <cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>

#include "cairo_ctx.h"
#include "comms.h"
#include "device.h"
#include "fontstash.h"
#include "script_ops.h"

typedef struct {
  GThread* main;
  GtkWidget* window;
  gboolean draw_queued;
  GMutex draw_mutex;
  GCond draw_finished;
  float last_x;
  float last_y;
} cairo_gtk_t;

cairo_gtk_t g_cairo_gtk;

extern device_info_t g_device_info;
extern device_opts_t g_opts;

static gpointer cairo_gtk_main(gpointer user_data)
{
  gtk_widget_show_all((GtkWidget*)g_cairo_gtk.window);
  gtk_main();

  return NULL;
}

static gboolean on_draw(GtkWidget* widget,
                        cairo_t* cr,
                        gpointer data)
{
  g_mutex_lock (&g_cairo_gtk.draw_mutex);

  scenic_cairo_ctx_t* p_ctx = (scenic_cairo_ctx_t*)data;
  cairo_set_source_surface(cr, p_ctx->surface, 0, 0);
  cairo_paint(cr);

  if (g_cairo_gtk.draw_queued) {
    g_cairo_gtk.draw_queued = FALSE;
    g_cond_signal(&g_cairo_gtk.draw_finished);
  }

  g_mutex_unlock(&g_cairo_gtk.draw_mutex);

  return FALSE;
}

static gboolean on_delete_event(GtkWidget* widget,
                                GdkEvent* event,
                                gpointer data)
{
  send_close(0);
  return TRUE;
}

int device_init(const device_opts_t* p_opts,
                device_info_t* p_info,
                driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("cairo %s", __func__);
  }

  scenic_cairo_ctx_t* p_ctx = scenic_cairo_init(p_opts, p_info);
  if (!p_ctx) {
    log_error("cairo %s failed", __func__);
    return -1;
  }
  p_ctx->cr = cairo_create(p_ctx->surface);

  gtk_init(NULL, NULL);

  g_cairo_gtk.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(g_cairo_gtk.window), "scenic_driver_local: cairo");
  gtk_window_set_default_size(GTK_WINDOW(g_cairo_gtk.window), p_info->width, p_info->height);
  gtk_window_set_resizable(GTK_WINDOW(g_cairo_gtk.window), FALSE);

  g_signal_connect(G_OBJECT(g_cairo_gtk.window), "delete-event", G_CALLBACK(on_delete_event), NULL);

  GtkDrawingArea* drawing_area = (GtkDrawingArea*)gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(g_cairo_gtk.window), (GtkWidget*)drawing_area);
  g_signal_connect((GtkWidget*)drawing_area, "draw", G_CALLBACK(on_draw), p_ctx);

  g_cairo_gtk.main = g_thread_new("gtk_main", cairo_gtk_main, NULL);

  return 0;
}

int device_close(device_info_t* p_info)
{
  if (g_opts.debug_mode) {
    log_info("cairo %s", __func__);
  }

  scenic_cairo_ctx_t* p_ctx = (scenic_cairo_ctx_t*)p_info->v_ctx;
  gtk_main_quit();
  g_thread_join(g_cairo_gtk.main);
  cairo_destroy(p_ctx->cr);
  scenic_cairo_fini(p_ctx);
}

void device_poll()
{
}

void device_begin_render(driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("cairo %s", __func__);
  }

  scenic_cairo_ctx_t* p_ctx = (scenic_cairo_ctx_t*)p_data->v_ctx;

  g_mutex_lock(&g_cairo_gtk.draw_mutex);

  g_cond_clear(&g_cairo_gtk.draw_finished);
  g_cairo_gtk.draw_queued = TRUE;

  // Paint surface to clear color
  cairo_set_source_rgba(p_ctx->cr,
                        p_ctx->clear_color.red,
                        p_ctx->clear_color.green,
                        p_ctx->clear_color.blue,
                        p_ctx->clear_color.alpha);
  cairo_paint(p_ctx->cr);
}

void device_end_render(driver_data_t* p_data)
{
  if (g_opts.debug_mode) {
    log_info("cairo %s", __func__);
  }

  scenic_cairo_ctx_t* p_ctx = (scenic_cairo_ctx_t*)p_data->v_ctx;

  cairo_surface_flush(p_ctx->surface);
  gtk_widget_queue_draw((GtkWidget*)g_cairo_gtk.window);

  while (g_cairo_gtk.draw_queued) {
    g_cond_wait(&g_cairo_gtk.draw_finished, &g_cairo_gtk.draw_mutex);
  }

  g_mutex_unlock(&g_cairo_gtk.draw_mutex);
}
