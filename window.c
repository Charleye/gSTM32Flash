/******************************************************************************
 * Main interface
 * ****************************************************************************
 * Copyright (C) 2016
 * Written by Kart (kartdream@163.com)
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 * ****************************************************************************
 */
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "window.h"
#include "parser.h"
#include "port.h"
#include "stm32.h"

/* global variable */
window_t *data;

char *devices_list[DEVICES] = {
	"/dev/ttyUSB0",
	"/dev/ttyUSB1",
	"/dev/ttyUSB2",
	"/dev/ttyUSB3",
	"/dev/ttyS0",
	"/dev/ttyS1",
	"/dev/ttyS2",
	"/dev/ttyS3"
//	"/dev/tts/0",
//	"/dev/tts/1",
//	"/dev/tts/2",
//	"/dev/tts/3",
//	"/dev/usb/tts/0",
//	"/dev/usb/tts/1",
//	"/dev/usb/tts/2",
//	"/dev/usb/tts/3"
};

char *baud_rate_list[BAUDRATE] = {
	"1200",
	"1800",
	"2400",
	"4800",
	"9600",
	"19200",
	"38400",
	"57600",
	"115200",
	"128000",
	"230400",
	"256000",
	"460800",
	"500000",
	"576000",
	"921600",
	"1000000",
	"1500000",
	"2000000"
};

int main(int argc, char **argv)
{
	GtkWidget *window;

	gtk_init(&argc, &argv);
	data = calloc (1, sizeof(window_t));

	window = create_window(data);
	
	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all(window);
	gtk_main();

	free (data);
	return 0;
}

GtkWidget* create_window(window_t* data)
{
	struct stat my_stat;
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *menubar;
	GtkWidget *prompt_box, *viewport,  *prom_text;
	GtkWidget *setup_label, *setup_box, *setup_port, *port, *setup_rate, *rate, *radio;
	GtkWidget *setup_file, *file_box, *file_label, *file_text, *file_button;
	GtkWidget *program_label, *program_box, *progress_label, *progress_bar, *button;
	GdkRGBA rgba, foreground;
	GtkAccelGroup *accel_group = NULL;
	GSList *group = NULL;

	accel_group = gtk_accel_group_new ();
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	gtk_widget_set_size_request(window, 550, 580);
	gtk_window_set_icon_from_file (GTK_WINDOW (window), "logo.png", NULL); 

	gtk_window_set_title(GTK_WINDOW(window), "STM32Flash");
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	data->window = window;	/* save the handler of window */

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0); /* main box */
	
	menubar = create_menu_bar(window, box, accel_group);
	gtk_box_pack_start (GTK_BOX(box), menubar, FALSE, FALSE, 0);

	prompt_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX(box), prompt_box, FALSE, FALSE, 0);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_ETCHED_OUT);

	prom_text = vte_terminal_new ();
	gdk_rgba_parse (&rgba, "rgb(255,255,255)");
	gdk_rgba_parse (&foreground, "rgb(0,0,0)");
	
	vte_terminal_set_size (VTE_TERMINAL (prom_text), 40, 18); 
	vte_terminal_set_colors_rgba (VTE_TERMINAL(prom_text), NULL, &rgba, &rgba, 0);
	vte_terminal_set_color_foreground_rgba (VTE_TERMINAL (prom_text), &foreground);
    vte_terminal_set_cursor_shape (VTE_TERMINAL (prom_text), VTE_CURSOR_SHAPE_IBEAM);
	vte_terminal_set_cursor_blink_mode (VTE_TERMINAL (prom_text), VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_color_cursor_rgba (VTE_TERMINAL (prom_text), &rgba);
	vte_terminal_set_scroll_on_output (VTE_TERMINAL (prom_text), TRUE);
	vte_terminal_set_backspace_binding(VTE_TERMINAL(prom_text), VTE_ERASE_ASCII_BACKSPACE);

	gtk_container_add (GTK_CONTAINER (viewport), prom_text);
	gtk_box_pack_start (GTK_BOX (prompt_box), viewport, FALSE, FALSE, 0);
	data->vte = prom_text;
	
	setup_label = gtk_label_new ("Serial Port Setup :");
	gtk_box_pack_start (GTK_BOX(box), setup_label, FALSE, TRUE, 0);
	gtk_widget_set_size_request (setup_label, 100, 30);
	gtk_misc_set_alignment (GTK_MISC (setup_label), 0.01, 0.5);
	
	setup_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX(box), setup_box, FALSE, TRUE, 0);
	
	setup_port = gtk_label_new ("Port");
	gtk_box_pack_start (GTK_BOX(setup_box), setup_port, FALSE, TRUE, 0);
	gtk_widget_set_size_request (setup_port, 60, -1);
	gtk_misc_set_alignment (GTK_MISC(setup_port), 0.8, 0.5);

	port = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX(setup_box), port, FALSE, TRUE, 0);
	gtk_widget_set_size_request (port, -1, -1);

	for ( int i = 0; i < DEVICES; i++) {
//		if (stat (devices_list[i], &my_stat) == 0)
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(port), devices_list[i]);
	}
//	gtk_combo_box_set_active (GTK_COMBO_BOX(port), 0);
	data->port = port;

	setup_rate = gtk_label_new ("Baud Rate");
	gtk_box_pack_start (GTK_BOX(setup_box), setup_rate, FALSE, TRUE, 0);
	gtk_widget_set_size_request (setup_rate, 80, -1);
	gtk_misc_set_alignment (GTK_MISC(setup_rate), 0.8, 0.5);

	rate = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX(setup_box), rate, FALSE, TRUE, 0);
	gtk_widget_set_size_request (rate, -1, -1);
	
	for( int i = 0; i < BAUDRATE; i++)
	{
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(rate), baud_rate_list[i]);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX(rate), 14);
	data->baudrate = rate;

	radio =	gtk_radio_button_new_with_label (NULL, "High-speed");
	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON(radio));
	radio = gtk_radio_button_new_with_label (group, "Low-speed");
	gtk_box_pack_start (GTK_BOX(setup_box), (g_slist_last(group))->data, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(setup_box), radio, FALSE, FALSE, 0);
	data->radio = radio;

	setup_file = gtk_label_new ("Select File :");
	gtk_box_pack_start (GTK_BOX(box), setup_file, FALSE, FALSE, 0);
	gtk_widget_set_size_request (setup_file, 100, 50);
	gtk_misc_set_alignment (GTK_MISC(setup_file), 0.01, 0.5);

	file_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX(box), file_box, FALSE, FALSE, 0);

	file_label = gtk_label_new ("File Name");
	gtk_box_pack_start (GTK_BOX(file_box), file_label, FALSE, FALSE, 0);
	gtk_widget_set_size_request (file_label, 80, -1);
	gtk_misc_set_alignment (GTK_MISC(file_label), 0.5, 0.5);

	file_text = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX(file_box), file_text, FALSE, FALSE, 0);
	gtk_widget_set_size_request (file_text, 400, 30);
	data->filename = file_text;

	file_button = gtk_button_new_with_mnemonic ("Select File");
	gtk_box_pack_start (GTK_BOX(file_box), file_button, FALSE, FALSE, 0);
	gtk_widget_set_size_request (file_button, 80, 30);

	program_label = gtk_label_new ("Program :");
	gtk_box_pack_start (GTK_BOX(box), program_label, FALSE, FALSE, 0);
	gtk_widget_set_size_request (program_label, 100, 50);
	gtk_misc_set_alignment (GTK_MISC(program_label), 0.01, 0.5);

	program_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX(box), program_box, FALSE, FALSE, 0);

	progress_label = gtk_label_new ("Progress");
	gtk_box_pack_start (GTK_BOX(program_box), progress_label, FALSE, FALSE, 0);
	gtk_widget_set_size_request (progress_label, 80, -1);
	gtk_misc_set_alignment (GTK_MISC(progress_label), 0.8, 0.5);

	progress_bar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX(program_box), progress_bar, FALSE, FALSE, 0);
	gtk_widget_set_size_request (progress_bar, 390, -1);
	gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR(progress_bar), TRUE);
	data->progressbar = progress_bar;

	button = gtk_button_new_with_mnemonic ("Download");
	gtk_box_pack_start (GTK_BOX(program_box), button, FALSE, FALSE, 0);
	gtk_widget_set_size_request (button, 90, 30);

	gtk_container_add (GTK_CONTAINER(window), box);

	g_signal_connect (file_button, "clicked",
					  G_CALLBACK (button_select_file_clicked),
					  file_text);
	g_signal_connect (button, "clicked",
					  G_CALLBACK (button_download_clicked),
					  NULL);
	g_signal_connect (port, "changed",
					  G_CALLBACK (port_changed_activate),
					  NULL);
	return window;
}

void port_changed_activate (GtkComboBox *combobox, gpointer user_data)
{
	struct stat my_stat;
	gchar *text;
	text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combobox));
	if (stat (text, &my_stat) == 0)
		printf("test\n");
	else
		printf("faliure\n");
}

GtkWidget* create_menu_bar(GtkWidget *window, GtkWidget *box, GtkAccelGroup *accel_group)
{
	GtkWidget *menubar;
	GtkWidget *file, *edit, *help;
	GtkWidget *file_menu, *edit_menu, *help_menu;
	GtkWidget *item_quit;
	GtkWidget *item_prefer;		/* the item of edit menushell: perferences */
	GtkWidget *about;

	menubar = gtk_menu_bar_new ();

	file = gtk_menu_item_new_with_mnemonic("_File");
	edit = gtk_menu_item_new_with_mnemonic("_Edit");
	help = gtk_menu_item_new_with_mnemonic("_Help");

	file_menu = gtk_menu_new ();
	edit_menu = gtk_menu_new ();
	help_menu = gtk_menu_new ();

	gtk_menu_item_set_submenu (GTK_MENU_ITEM(file), file_menu);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM(edit), edit_menu);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM(help), help_menu);

	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), file);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), edit);
	gtk_menu_shell_append (GTK_MENU_SHELL (menubar), help);

	/* Create file menu item*/
	item_quit = gtk_menu_item_new_with_label ("Quit");
	gtk_widget_add_accelerator (item_quit, "activate", accel_group,
			GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_shell_append (GTK_MENU_SHELL (file_menu), item_quit);

	/* Create Edit menu item*/
	item_prefer = gtk_menu_item_new_with_label("Preferences");
	gtk_widget_add_accelerator (item_prefer, "activate", accel_group,
			GDK_KEY_s, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_menu_shell_append (GTK_MENU_SHELL (edit_menu), item_prefer);

	/* Create help menu item */
	about = gtk_menu_item_new_with_label ("About");
	gtk_menu_shell_append (GTK_MENU_SHELL (help_menu), about);

	/* signal connect */
	g_signal_connect(about, "activate",
			 G_CALLBACK (menu_about_activate), NULL);

	g_signal_connect(item_quit, "activate",
			 G_CALLBACK (menu_quit_activate), NULL);

	g_signal_connect (item_prefer, "activate",
			G_CALLBACK (menu_preferences_activate), NULL);
	return menubar;
}

void menu_quit_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new (GTK_WINDOW (data->window),
									 GTK_DIALOG_MODAL,
									 GTK_MESSAGE_QUESTION,
									 GTK_BUTTONS_OK_CANCEL,
									"Are you sure?");

	gtk_widget_set_size_request (dialog, 350, 120);
	gtk_widget_show_all (dialog);
	if(gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		gtk_main_quit ();
	}
	gtk_widget_destroy (dialog);
}

void menu_about_activate (GtkMenuItem *menuitem, gpointer user_data)
{
}

void menu_preferences_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	char buf[100];
	sprintf (buf, "menu item: preferences executed\n\r");
	vte_terminal_feed (VTE_TERMINAL(data->vte), buf, strlen (buf));
}
void button_select_file_clicked (GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog;
	GtkFileFilter *file_filter_bin;
	GtkFileFilter *file_filter_hex;
	GtkFileFilter *file_filter_all;
	
	data->file_opt = 0;
	
	dialog = gtk_file_chooser_dialog_new ("Select File",
										  GTK_WINDOW(data->window),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
										  "_Cancle", GTK_RESPONSE_CANCEL,
										  "_OK", GTK_RESPONSE_ACCEPT,
										  NULL);

	file_filter_bin = gtk_file_filter_new ();
	file_filter_hex = gtk_file_filter_new ();
	file_filter_all = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter_bin, "*.bin");
	gtk_file_filter_add_pattern (file_filter_bin, "*.bin");
	gtk_file_filter_add_pattern (file_filter_bin, "*.BIN");

	gtk_file_filter_set_name (file_filter_hex, "*.hex");
	gtk_file_filter_add_pattern (file_filter_hex, "*.hex");
	gtk_file_filter_add_pattern (file_filter_hex, "*.HEX");

	gtk_file_filter_set_name (file_filter_all, "all");
	gtk_file_filter_add_pattern (file_filter_all, "*");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), file_filter_hex);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), file_filter_bin);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), file_filter_all);

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), "./");

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) 
	{
		gchar *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text (GTK_ENTRY (user_data), filename);
	}
	gtk_widget_destroy (dialog);
}

void button_download_clicked (GtkButton *button, gpointer user_data)
{
	pthread_t	wr;
	int err;
//	void *tret;

	if((err = pthread_create (&wr, NULL, write_flash, NULL)) != 0)
		printf("Can't create thread wr.\n");
//	if((err = pthread_join (wr, &tret)) != 0)
//		printf("Can't join with thread wr.\n");
//	printf("Thread wr exit code %ld.\n", (long)tret);
}

extern parser_ops_t PARSER_HEX;
struct port_options port_opts = {
	.device				= NULL,
	.baudrate			= SERIAL_BAUD_576000,
	.serial_mode		= "8e1",
	.bus_addr			= 0,
	.rx_frame_max		= STM32_MAX_RX_FRAME,
	.tx_frame_max		= STM32_MAX_TX_FRAME,
};

void* write_flash (void *user_data)
{
	const char			*filename;
	char				buf[1000];
	parser_t			parser_err;
	stm32_t				stm_err;

	port_interface_t	*port		= NULL;
	parser_ops_t		*parser		= NULL;	
	stm32_struct_t		*stm		= NULL;
	void				*storage	= NULL;

	vte_terminal_reset (VTE_TERMINAL (data -> vte), TRUE, TRUE);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (data -> progressbar), 0);
	if (gtk_entry_get_text_length (GTK_ENTRY (data -> filename)) != 0)
	{
		filename = gtk_entry_get_text (GTK_ENTRY (data -> filename));

		parser = &PARSER_HEX;
		if((storage = parser -> init ()) == NULL)
		{
			sprintf (buf, "Failed to initialize parser %s.\n\r", parser -> name);
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
			goto close;
		}
		if((parser_err = parser -> open (storage, filename)) != PARSER_OK)
		{
			sprintf (buf, "Failed to open the file: %s.\n\r", g_path_get_basename (filename));
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
			goto close;
		}

		sprintf (buf, "Using Parser : %s\n\r", parser -> name);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		
		port_opts.device = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (data -> port));
		if ( port_open (&port_opts, &port) != PORT_OK)
		{
			sprintf (buf, "Failed to open the port: %s\n\r", port_opts.device);
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
			goto close;
		}
		sprintf (buf, "Interface %s: %s\n\r", port -> name, port ->get_cfg_str (port));
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));

		if ((stm = stm32_init (port)) == NULL)
		{
			sprintf (buf, "Failed to initialize stm32 device.\n\r");
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
			goto close;
		}

		sprintf (buf, "Version		: 0x%02x\n\r", stm->bl_version);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		if (port -> flags & PORT_GVR_ETX)
		{
			sprintf (buf, "Option 1	: 0x%02x\n\r", stm->option1);
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
			sprintf (buf, "Option 2	: 0x%02x\n\r", stm->option2);
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		}
		sprintf (buf, "Device ID	: 0x%04x (%s)\n\r", stm->pid, stm->dev->name);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		sprintf (buf, "- RAM		: %dKiB (%db reserved by bootloader)\n\r", (stm->dev->ram_end - 0x20000000) / 1024, stm->dev->ram_start - 0x20000000);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		sprintf (buf, "- Flash		: %dKiB (sector size: %dx%d)\n\r", (stm->dev->fl_end - stm->dev->fl_start) / 1024, stm->dev->fl_pps, stm->dev->fl_ps);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		sprintf (buf, "- Option RAM	: %db\n\r", stm->dev->opt_end - stm->dev->opt_start + 1);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		sprintf (buf, "- System RAM	: %dKiB\n\r", (stm->dev->mem_end - stm->dev->mem_start) / 1024);
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));

		uint8_t data_buf[10000];
		uint32_t addr, start, end;
		uint32_t first_page, num_page;

		start = stm->dev->fl_start;
		end = stm->dev->fl_end;
		first_page = 0;
		num_page = 0xff;	/* mass erase */

		sprintf (buf, "Write data to flash memory.\n\r");
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		
		off_t	offset = 0;
		unsigned int size, len;
		unsigned int max_wlen, max_rlen;

		max_wlen = port_opts.tx_frame_max - 2;	/* skip len and crc */
		max_wlen &= ~3;		/* 32 bit aligned*/

		max_rlen = port_opts.rx_frame_max;
		max_rlen = max_rlen < max_wlen ? max_rlen : max_wlen;

		size = parser -> size (storage);

		sprintf (buf, "Erasing flash memory.\n\r");
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		if ((stm_err = stm32_erase_memory (stm, first_page, num_page)) != STM32_OK)
		{
			sprintf (buf, "Faild to erase flash memory.\n\r");
			vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
		}
		
		addr = start;
		while (addr < end && offset < size)
		{
			uint32_t left = end - addr;
			len = max_wlen > left ? left : max_wlen;
			len = len > size - offset ? size - offset : len;

			if (parser -> read (storage, data_buf, &len) != PARSER_OK)
				goto close;
			if (len == 0)
			{
				sprintf (buf, "Failed to read %s file.\n\r", parser -> name);
				vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
				goto close;
			}

			if ((stm_err = stm32_write_memory (stm, addr, data_buf, len)) != STM32_OK)
			{
				sprintf (buf, "Failed to write flash memory at address 0x%08x.\n\r", addr);
				vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
				goto close;
			}

			addr += len;
			offset += len;
			gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (data -> progressbar), (1.0 / size) * offset);
		}
		sprintf (buf, "Done!\n\r");
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
	}
	else
	{
		sprintf (buf, "Failed to choose file.\n\r");
		vte_terminal_feed (VTE_TERMINAL (data -> vte), buf, strlen(buf));
	}
close:
	if(storage)			parser -> close (storage);
	if(stm)				stm32_close (stm);
	if(port)			port -> close (port);
	pthread_exit ((void*)0);
}
