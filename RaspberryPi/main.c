 //Troy Fine
//CMPE 121
//Lab Project - Oscilloscope
//11.20.17
//Main Control Loop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <wiringPi.h>
#include <errno.h>
#include <libusb.h>
#include <shapes.h>
#include <fontinfo.h>
#include <math.h>

#define PI 3.14159265

typedef struct{
    VGfloat x, y;
} data_point;

char* mode;
int triggerL;
char* triggerS;
int triggerC;
int xscale;
int yscale;
int peak = 255;
int shift=0;

char channel1_data[100000]; // Data samples from Channel 1
char channel2_data[100000]; // Data samples from Channel 2
char pot1Val[2];

///////////////////////////////////////CHECK THE COMMAND-LINE ARGUMENTS/////////////////////////////////
int argCheck (int argc, char * argv[]){
    
    if((argc > 13) || (argc % 2 == 0)){
        printf("These settings can be configured in any order: %s -m <mode> -t <trigger> -s <trigger-slope> -c <trigger-channel> -x <xscale> -y <yscale>\n", argv[0]);
        exit(1);
    }
    
    int i;
    int value;
    for(i = 1; i < argc-1;){
        if(strcmp(argv[i], "-m") == 0){ //free or trigger
            if((strcmp(argv[i+1], "free") == 0) || (strcmp(argv[i+1], "trigger") == 0)){
                mode = argv[++i];
                if(i < argc - 1)
                    ++i;
            }
            else{
                printf("-m <mode>: <mode> can be either free or trigger.\n");
                exit(-1);
            }
            
        }
        else if(strcmp(argv[i], "-t") == 0){
            value = atoi(argv[i+1]);
            if((value >= 0) && (value <= 5000) && (value % 100 == 0)){
                ++i;
                triggerL = value;
                if(i < argc - 1)
                    ++i;
            }
            else{
                printf("-t <trigger-level>: <trigger-level> can range from 0 to 5000 in steps of 100.\n");
                exit(-1);
            }
            
        }
        
        else if(strcmp(argv[i], "-s") == 0){ //pos or neg
            if((strcmp(argv[i+1], "pos") == 0) || (strcmp(argv[i+1], "neg") == 0)){
                triggerS = argv[++i];
                if(i < argc - 1)
                    ++i;
            }
            else{
                printf("-s <trigger-slope>: <trigger-slope> can be either pos or neg.\n");
                exit(-1);
            }
            
        }
        else if(strcmp(argv[i], "-c") == 0){
            value = atoi(argv[i+1]);
            if((value == 1) || (value == 2)){
                ++i;
                triggerC = value;
                if(i < argc - 1)
                    ++i;
            }
            else{
                printf("-c <trigger-channel>: <trigger-channel> can be either 1 or 2.\n");
                exit(-1);
            }
            
        }
        else if(strcmp(argv[i], "-x") == 0){
            value = atoi(argv[i+1]);
            if((value == 1) || (value == 10) || (value == 100) || (value == 500) || (value == 1000) || (value == 2000) || (value == 5000) || (value == 10000) || (value == 50000) || (value == 100000)){
                ++i;
                xscale = value;
                if(i < argc - 1)
                    ++i;
            }
            else{
                printf("-x <xscale>: <xscale> can be 1, 10, 100, 500, 1000, 2000, 5000, 10000, 50000, or 100000.\n");
                exit(-1);
            }
            
        }
        
        else if(strcmp(argv[i], "-y") == 0){
            value = atoi(argv[i+1]);
            if((value == 100) || (value == 500) || (value == 1000) || (value == 2000) || (value == 2500)){
                ++i;
                yscale = value;
                if(i < argc - 1)
                    ++i;
            }
            else{
                printf("-y <yscale>: <yscale> can be 100, 500, 1000, 2000, or 2500.\n");
                exit(-1);
            }
            
        }
        else {
            printf("Configurable settings include: -m <mode> -t <trigger> -s <trigger-slope> -c <trigger-channel> -x <xscale> -y <yscale>\n");
            exit(-1);
        }
    }
    return 0;
}

// wait for a specific key pressed
void waituntil(int endchar) {
    int key;
    
    for (;;) {
        key = getchar();
        if (key == endchar || key == '\n') {
            break;
        }
    }
}


// Draw grid lines
void grid(VGfloat x, VGfloat y, // Coordinates of lower left corner
          int nx, int ny, // Number of x and y divisions
          int w, int h) // screen width and height

{
    VGfloat ix, iy;
    Stroke(128, 128, 128, 0.5); // Set color
    StrokeWidth(2); // Set width of lines
    for (ix = x; ix <= x + w; ix += w/nx) {
        Line(ix, y, ix, y + h);
    }
    
    for (iy = y; iy <= y + h; iy += h/ny) {
        Line(x, iy, x + w, iy);
    }
}


// Draw the background for the oscilloscope screen
void drawBackground(int w, int h, // width and height of screen
                    int xdiv, int ydiv,// Number of x and y divisions
                    int margin){ // Margin around the image
    VGfloat x1 = margin;
    VGfloat y1 = margin;
    VGfloat x2 = w - 2*margin;
    VGfloat y2 = h - 2*margin;
    
    Background(128, 128, 128);
    
    Stroke(204, 204, 204, 1);
    StrokeWidth(1);
    
    Fill(44, 77, 120, 1);
    Rect(10, 10, w-20, h-20); // Draw framing rectangle
    
    grid(x1, y1, xdiv, ydiv, x2, y2); // Draw grid lines
}

// Display x and scale settings
void printScaleSettings(int xscale, int yscale, int xposition, int yposition, VGfloat tcolor[4]) {
    char str[100];
    
    setfill(tcolor);
    if (xscale >= 1000)
        sprintf(str, "X scale = %0d ms/div", xscale/1000);
    else
        sprintf(str, "X scale = %0d us/div", xscale);
    Text(xposition, yposition, str, SansTypeface, 18);
    
    sprintf(str, "Y scale = %3.2f V/div", yscale * 0.25);
    Text(xposition, yposition-50, str, SansTypeface, 18);
}

void processSamples(char *data, // sample data
                    int nsamples, // Number of samples
                    int xstart, // starting x position of wave
                    int xfinish, // Ending x position of wave
                    float yscale, // y scale in pixels per volt
                    data_point *point_array){
    VGfloat x1, y1;
    data_point p;
    
    for (int i=0; i< nsamples+64; i++){
        x1 = xstart + (xfinish-xstart)*i/nsamples;
        y1 = data[i+64] * 5 * yscale/256;
        p.x = x1;
        p.y = y1;
        point_array[i+64] = p;
    }
}


///////////////////Convert waveform samples into screen coordinates FOR TRIGGERED WAVEFORM////////
void processSamples_trigger(char *data, // sample data
                    int nsamples, // Number of samples
                    int xstart, // starting x position of wave
                    int xfinish, // Ending x position of wave
                    float yscale, // y scale in pixels per volt
                    data_point *point_array){
    VGfloat x1, y1;
    data_point p;
	int trig = triggerL / 1000;
	peak = 255;
	peak /= 4;
	int level = trig * peak;
	shift = 0;
	int count = 0;

	///////////////////////FOR POSITIVE TRIGGER////////////////////
	if((strcmp(triggerS, "pos") == 0)){

		for(int j = 64; j < nsamples+64; j++){
			if ((data[j-1] > level) && (data[j] <= level) && (count == 0)){ 
				shift = j;
				++count;	
			}

		}	
	}
	
    ///////////////////////FOR NEGATIVE TRIGGER////////////////////
    if((strcmp(triggerS, "neg") == 0)){

        for(int j = 64; j < nsamples+64; j++){
            if ((data[j-1] < level) && (data[j] >= level) && (count == 0)){
                shift = j;
                ++count;
            }

        }
    }
	/////////////////////////TRIGGER WAS FOUND////////////////////////////
	if(count != 0){
		for (int i=0; i< nsamples+ shift; i++){
        	x1 = xstart + (xfinish-xstart)*i/nsamples;
        	y1 = data[i+shift] * 5 * yscale/256;
        	p.x = x1;
        	p.y = y1;
        	point_array[i+shift] = p;
    	}
	}
	////////////////////////TRIGGER WAS NOT FOUND///////////////////////
	else if (count == 0){
		for (int i=0; i< nsamples+64; i++){
        	x1 = xstart + (xfinish-xstart)*i/nsamples;
        	y1 = data[i+64] * 5 * yscale/256;
        	p.x = x1;
        	p.y = y1;
        	point_array[i+64] = p;
    	}
		shift = 64;
	}
	
}

////////////////////////////PLOT FREE RUNNING WAVEFORM//////////////////////////////
void plotWave_free(data_point *data, // sample data
              int nsamples, // Number of samples
              int yoffset, // y offset from bottom of screen
              VGfloat linecolor[4] // Color for the wave
){
    
    data_point p;
    VGfloat x1, y1, x2, y2;
    
    Stroke(linecolor[0], linecolor[1], linecolor[2], linecolor[3]);
    StrokeWidth(4);
    
    p = data[64];
    x1 = p.x;
    y1 = p.y + yoffset;
    
    for (int i=0; i< nsamples+64; i++){
        p = data[i+64];
        x2 = p.x;
        y2 = p.y + yoffset;
        Line(x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
    }
}

/////////////////////////PLOT TRIGGERED WAVEFORM//////////////////////
void plotWave(data_point *data, // sample data
              int nsamples, // Number of samples
              int yoffset, // y offset from bottom of screen
              VGfloat linecolor[4] // Color for the wave
){

    data_point p;
    VGfloat x1, y1, x2, y2;

    Stroke(linecolor[0], linecolor[1], linecolor[2], linecolor[3]);
    StrokeWidth(4);

    p = data[shift];
    x1 = p.x;
    y1 = p.y + yoffset;

    for (int i=0; i< nsamples+shift; i++){
        p = data[i+shift];
        x2 = p.x;
        y2 = p.y + yoffset;
        Line(x1, y1, x2, y2);
        x1 = x2;
        y1 = y2;
    }
}


/////////////////////////////////////MAIN CONTROL LOOP//////////////////////////////
int main (int argc, char * argv[]){
    
    int sent_bytes; // Bytes transmitted
    int rcvd_bytes; // Bytes received
    int return_val;
    
    //Command-line argument parameters
    mode = "trigger";
    triggerL = 2500;
    triggerS = "pos";
    triggerC = 1;
    xscale = 1000;
    yscale = 1000;
    
    argCheck(argc, argv); //Check the command-line arguments
    
    libusb_device_handle* dev; // Pointer to data structure representing USB device
    
    libusb_init(NULL); // Initialize the LIBUSB library
    
    // Open the USB device (the Cypress device has
    // Vendor ID = 0x04B4 and Product ID = 0x8051)
    dev = libusb_open_device_with_vid_pid(NULL, 0x04B4, 0x8051);
    
    if (dev == NULL){
        perror("device not found\n");
    }
    
    // Reset the USB device.
    // This step is not always needed, but clears any residual state from
    // previous invocations of the program.
    if (libusb_reset_device(dev) != 0){
        perror("Device reset failed\n");
    }
    
    // Set configuration of USB device
    if (libusb_set_configuration(dev, 1) != 0){
        perror("Set configuration failed\n");
    }
    
    // Claim the interface.  This step is needed before any I/Os can be
    // issued to the USB device.
    if (libusb_claim_interface(dev, 0) !=0){
        perror("Cannot claim interface");
    }
    
    /////////////////////////Set up and draw the display/////////////////
    
    int width, height; // Width and height of screen in pixels
    int margin = 10; // Margin spacing around screen
    int xdivisions = 10; // Number of x-axis divisions
    int ydivisions = 8; // Number of y-axis divisions
    char str[100];
    
    VGfloat textcolor[4] = {0, 200, 200, 0.5}; // Color for displaying text
    VGfloat wave1color[4] = {240, 0, 0, 0.5}; // Color for displaying Channel 1 data
    VGfloat wave2color[4] = {200, 200, 0, 0.5}; // Color for displaying Channel 2 data
    
    data_point channel1_points[100000];
    data_point channel2_points[100000];
    
    int xstart = margin;
    int xlimit = width - 2*margin;
    int ystart = margin;
    int ylimit = height - 2*margin;
    
    int pixels_per_volt = (ylimit-ystart)*50/(yscale/100);
    
	init(&width, &height); // Initialize display and get width and height
    Start(width, height);


    while(1){
        // Perform the IN transfer for wave 1 (from device to host).
        // This will read the data back from the device.
        return_val = libusb_bulk_transfer(dev, // Handle for the USB device
                                          (0x01 | 0x80), // Address of the Endpoint in USB device
                                          // MS bit nust be 1 for IN transfers
                                          channel1_data, // address of receive data buffer
                                          10064, // Size of data buffer
                                          &rcvd_bytes, // Number of bytes actually received
                                          0 // Timeout in milliseconds (0 to disable timeout)
                                          );
	    //Perform the IN transfer for wave 2 (from device to host).
        // This will read the data back from the device.
        return_val = libusb_bulk_transfer(dev, // Handle for the USB device
                                          (0x02 | 0x80), // Address of the Endpoint in USB device
                                          // MS bit nust be 1 for IN transfers
                                          channel2_data, // address of receive data buffer
                                          10064, // Size of data buffer
                                          &rcvd_bytes, // Number of bytes actually received
                                          0 // Timeout in milliseconds (0 to disable timeout)
                                          );		
        
		//Perform the IN transfer from the pot to control wave 1  (from device to host).
        // This will read the data back from the device.
        return_val = libusb_bulk_transfer(dev, // Handle for the USB device
                                          (0x03 | 0x80), // Address of the Endpoint in USB device
                                          // MS bit nust be 1 for IN transfers
                                          pot1Val, // address of receive data buffer
                                          2, // Size of data buffer
                                          &rcvd_bytes, // Number of bytes actually received
                                          0 // Timeout in milliseconds (0 to disable timeout)
                                          );

 
		for(int i = 0; i < 64; ++i){
        	//printf("%d\n", channel1_data[i]);
		}

		WindowClear();
        Start(width, height);
        drawBackground(width, height, xdivisions, ydivisions, margin);
        printScaleSettings(xscale, yscale/250, width-300, height-50, textcolor);
        
		if((strcmp(mode, "trigger") == 0)){ //channel 1 wave is being triggered
			if (triggerC == 1){
				processSamples_trigger(channel1_data, (xscale*xdivisions)/50, margin, width, pixels_per_volt, channel1_points);
        		processSamples(channel2_data, (xscale*xdivisions)/50, margin, width, pixels_per_volt, channel2_points);
				plotWave(channel1_points, (xscale*xdivisions)/50, 650+pot1Val[0], wave1color);
				plotWave_free(channel2_points, (xscale*xdivisions)/50, 650+pot1Val[1], wave2color);
			}
			else if(triggerC == 2){ //channel 2 wave is being triggered
				processSamples(channel1_data, (xscale*xdivisions)/25, margin, width, pixels_per_volt, channel1_points);
				processSamples_trigger(channel2_data, (xscale*xdivisions)/25, margin, width, pixels_per_volt, channel2_points);
				plotWave_free(channel1_points, (xscale*xdivisions)/25, 650+pot1Val[0], wave1color);
				plotWave(channel2_points, (xscale*xdivisions)/25, 650+pot1Val[1], wave2color);
			}
		}
		else if((strcmp(mode, "free") == 0)){ //both waves are free-running
			processSamples(channel1_data, (xscale*xdivisions)/25, margin, width, pixels_per_volt, channel1_points);
            processSamples(channel2_data, (xscale*xdivisions)/25, margin, width, pixels_per_volt, channel2_points);
			plotWave_free(channel1_points, (xscale*xdivisions)/25, 650+((pot1Val[0]*height)/255), wave1color);
			plotWave_free(channel2_points, (xscale*xdivisions)/25, 650+((pot1Val[1]*height)/255), wave2color);
		}
               
        End();
    }
    
    libusb_close(dev);
    return 0;
}
