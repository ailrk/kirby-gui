#include <stdio.h>
#include <gtk/gtk.h>
#include "kirby.h"


static void activate (GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "kbgui");
    gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
    gtk_window_present (GTK_WINDOW (window));
}


int main (int argc, char *argv[]) {
    GtkApplication *app;
    int status;

    app = gtk_application_new ("org.kbgui", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_print ("Hello");
    g_object_unref (app);
    return 0;
}
