/******************************************************************************
 * main interface header file
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
#ifndef _MAIN_H
#define _MAIN_H

#define DEVICES 8
#define BAUDRATE 19

typedef struct termios termios_info_t;

typedef struct
{
	int file_opt;				//indicate that the file has been choosed
	int port_stat;				//the state of the port
	int rec_count;				//the amount of byte has been received
	int send_count;				//the number of byte has been send

	GtkWidget *window;
	GtkWidget *vte;
	GtkWidget *choice;
	GtkWidget *port;
	GtkWidget *baudrate;
	GtkWidget *radio;
	GtkWidget *filename;
	GtkWidget *progressbar;

}window_t;

/* window function */
GtkWidget* create_window(window_t*);
GtkWidget* create_menu_bar(GtkWidget*, GtkWidget*, GtkAccelGroup*);

/* signal callback function */
void menu_quit_activate (GtkMenuItem*, gpointer);
void menu_about_activate (GtkMenuItem*, gpointer);
void menu_preferences_activate (GtkMenuItem*, gpointer);
void button_select_file_clicked (GtkButton*, gpointer);
void button_download_clicked (GtkButton*, gpointer);
void port_changed_activate (GtkComboBox*, gpointer);

void* write_flash (void* user_data);
#endif
