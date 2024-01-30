/*
 *Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *Permission to use, copy, modify, and/or distribute this software for any
 *purpose with or without fee is hereby granted, provided that the above
 *copyright notice and this permission notice appear in all copies.
 *
 *THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE
 */

#include <gps.h>
#include <gpsdclient.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MODE_STRING_COUNT 4
#define LOCAL_HOST "127.0.0.1"
#define SOC_PORT 9090

typedef enum{
        other,
        gps,
        mobile
}locationMethod;

typedef struct locationInfo {
        double latitude;
        double longitude;
        double height;
        double vertical_uncertainity;
        double major_axis;
        double minor_axis;
        double orientation;
        locationMethod method;
}locationInfo;

int main(void)
{
	struct gps_data_t gps_info;
	struct fixsource_t gpsd_source;
	int status, flags,indoor_dep, orientation;
	bool latlon_set = false, alti_set = false, eph_set = false, epv_set = false, is_data_valid = false;
	double ehpe = 0, epv = 0;
	int cliSocket;
	locationInfo loc_info;
	int bytes_sent = 0;
	struct sockaddr_in address;

	(void)gpsd_source_spec(NULL, &gpsd_source);

	flags = WATCH_ENABLE | WATCH_JSON;

	status = gps_open(gpsd_source.server, gpsd_source.port, &gps_info);

	if (status != 0) {
		printf("No GPSD running or network error:%d,%s\n",errno,gps_errstr(errno));
		exit(EXIT_FAILURE);
	}

	(void)gps_stream(&gps_info, flags, gpsd_source.device);

	/* Wait for data from GPSD for a maximum of 10 seconds */
	while ((gps_waiting(&gps_info, 10000000)) && (is_data_valid == false)) {
		if (-1 == gps_read(&gps_info, NULL, 0))
		{
			printf("Read failure!\n");
			exit(EXIT_FAILURE);
		}
		if (PACKET_SET == (PACKET_SET & gps_info.set))
		{
			if (MODE_SET != (MODE_SET & gps_info.set)) {
				continue;
			}
		/* Checking if the mode is within the range */
			if (gps_info.fix.mode < 0 ||
			 gps_info.fix.mode >= MODE_STRING_COUNT)
			{
				gps_info.fix.mode = 0;
			}
			/* Updating the indoor deployment value to outdoor(2)*/
			indoor_dep = 2;
			/* Updating the orientation to zero degree as the horizontal area covered by GPS is a circle */
			orientation = 0;
			if (LATLON_SET == (LATLON_SET & gps_info.set))
			{
				if (isfinite(gps_info.fix.latitude) && isfinite(gps_info.fix.longitude))
				{
					latlon_set = true;
					/* Display data from the GPS receiver if valid */
					printf("\nLatitude: %.7f Longitude: %.7f\n",
					gps_info.fix.latitude, gps_info.fix.longitude);
				}
				else
				{
					latlon_set = false;
					printf("\nLatitude and Longitude: Data not found\n");
				}
			}
			else
			{
				latlon_set = false;
				printf("\nLatLon not set!\n");
			}
			if (ALTITUDE_SET == (ALTITUDE_SET & gps_info.set))
			{
				if (isfinite(gps_info.fix.altMSL))
				{
					alti_set = true;
					printf("Height: %.2f \n",gps_info.fix.altMSL);
				}
				else
				{
					alti_set = false;
					printf("Height: Data not found\n");
				}
			}
			else
			{
				alti_set = false;
				printf("Altitude not set\n");
			}
			if (isfinite(gps_info.fix.eph))
			{
				eph_set = true;
				ehpe = (gps_info.fix.eph)*1.7;
				printf("Major axis: %.2f\n", ehpe);
				printf("Minor axis: %.2f\n", ehpe);
			}
			else
			{
				eph_set = false;
				printf("Major/Minor axis : Data not found\n");
			}
			if (isfinite(gps_info.fix.epv))
			{
				epv_set = true;
				epv = (gps_info.fix.epv)*2;
				printf("Vertical Uncertainty: %.2f\n", epv);
			}
			else
			{
				epv_set = false;
				printf("vertical Uncertainty: Data not found\n");
			}
			printf("Indoor_dep: %d \n",indoor_dep);
			printf("Orientation: %d \n",orientation);
			if ((latlon_set == true) && (alti_set == true) && (eph_set == true) && (epv_set == true))
			{
				is_data_valid = true;
			}
		}
		else
		{
			printf("No data packets received...\n");
		}

		if ((cliSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			printf( "\n cliSocket creation error \n");
			return -1;
		}
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = inet_addr(LOCAL_HOST);
		address.sin_port = htons(SOC_PORT);
		loc_info.latitude = gps_info.fix.latitude;
		loc_info.longitude = gps_info.fix.longitude;
		loc_info.height = gps_info.fix.altMSL;
		loc_info.vertical_uncertainity = epv;
		loc_info.major_axis = ehpe;
		loc_info.minor_axis = ehpe;
		loc_info.orientation = 0;
		loc_info.method = gps;
		bytes_sent = sendto(cliSocket, (const char *)&loc_info, sizeof(loc_info), MSG_CONFIRM, (const struct sockaddr *) &address, sizeof(address));
		printf("%d bytes_sent to Wifi location app\n", bytes_sent);
	}
	sleep(1);
	flags = WATCH_DISABLE;
	(void)gps_stream(&gps_info, flags, gpsd_source.device);
	(void)gps_close(&gps_info);
	if (is_data_valid == true)
	{
		printf("GPS Data fields are set...Exiting!!\n");
		exit(EXIT_SUCCESS);
	}
	else
	{
		printf("GPS Wait timed out!!\n");
		exit(EXIT_FAILURE);
	}
}

