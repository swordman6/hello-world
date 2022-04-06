#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "rgb2yuv.h"

/*****************************************************************************************
 * YUV的格式有两大类：planar和packed。
 * 对于planar的YUV格式，先连续存储所有像素点的Y，紧接着存储所有像素点的U，随后是所有像素点的V。
 * 对于packed的YUV格式，每个像素点的Y、U、V都是连续交叉存储的。
 * 
 * YUV420p：又叫planer平面模式，Y ，U，V分别再不同平面，也就是有三个平面。
 * YUV420sp：又叫bi-planer或two-planer双平面，Y一个平面，UV在同一个平面交叉存储。
 * 
 * YUV444   Y:Cb:Cr = 1:1:1
 * YUV422   Y:Cb:Cr = 2:1:1
 * YUV420   Y:Cb:Cr = 4:1:1
 * 
 *.wav 音频文件 等于 PCM采样+header
 *.bmp 图像文件 等于 RGB色彩+header
 * 
 * *****************************************************************************************/

int simplest_yuv420_split(char *url, int w, int h,int num)
{
    if(url == NULL)
        return -1;

	FILE *fp = fopen(url,"r");
    if(fp == NULL){
        perror("fp error");
        return -1;
    }
	FILE *fp1 = fopen("output_420_y.y","w+");
    if(fp1 == NULL){
        fprintf(stdout, "%s\n", strerror(errno));
        return -1;
    }
	FILE *fp2 = fopen("output_420_u.y","w+");
    if(fp2 == NULL){
        fprintf(stdout, "%s\n", strerror(errno));
        return -1;
    }
	FILE *fp3 = fopen("output_420_v.y","w+");
    if(fp3 == NULL){
        fprintf(stdout, "%s\n", strerror(errno));
        return -1;
    }
 
	unsigned char *pic = (unsigned char *)malloc(w*h*3/2);
    if(pic == NULL){
        fclose(fp);
	    fclose(fp1);
	    fclose(fp2);
	    fclose(fp3);
        return -1;
    }
 
	for(int i=0; i<num; i++){
		fread(pic, 1, w*h*3/2, fp);

        //Gray remove the color
		//memset(pic+w*h,128,w*h/2);

		//Y
		fwrite(pic, 1, w*h, fp1);
		//U
		fwrite(pic+w*h, 1, w*h/4, fp2);
		//V
		fwrite(pic+w*h*5/4, 1, w*h/4, fp3);
	}
 
	free(pic);
	fclose(fp);
	fclose(fp1);
	fclose(fp2);
	fclose(fp3);
 
	return 0;
}

int simplest_yuv420_graybar(int width, int height,int ymin,int ymax,int barnum,char *url_out)
{
    if(url_out == NULL)
        return -1;

	int barwidth;
	float lum_inc;
	unsigned char lum_temp;
	int uv_width,uv_height;
	FILE *fp = NULL;
	unsigned char *data_y = NULL;
	unsigned char *data_u = NULL;
	unsigned char *data_v = NULL;
	int t = 0, i = 0, j = 0;
 
	barwidth  = width/barnum;
	lum_inc   = ((float)(ymax-ymin))/((float)(barnum-1));
	uv_width  = width/2;
	uv_height = height/2;
 
	data_y = (unsigned char *)malloc(width*height);
	data_u = (unsigned char *)malloc(uv_width*uv_height);
	data_v = (unsigned char *)malloc(uv_width*uv_height);
 
	if((fp=fopen(url_out,"w+"))==NULL){
		printf("Error: Cannot create file!");
		return -1;
	}
 
	//Output Info
	printf("Y, U, V value from picture's left to right:\n");
	for( t = 0; t < (width/barwidth); t++){
		lum_temp = ymin + (char)(t*lum_inc);
		printf("%3d, 128, 128\n",lum_temp);
	}
	//Gen Data
	for( j = 0; j < height; j++){
		for( i = 0; i < width; i++){
			t = i/barwidth;
			lum_temp = ymin+(char)(t*lum_inc);
			data_y[j*width+i] = lum_temp;
		}
	}
	for( j = 0; j < uv_height; j++){
		for( i=0; i < uv_width; i++){
			data_u[j*uv_width+i ]= 128;
		}
	}
	for( j=0; j < uv_height; j++){
		for( i=0; i < uv_width; i++){
			data_v[j*uv_width+i] = 128;
		}
	}

	fwrite(data_y, width*height, 1, fp);
	fwrite(data_u, uv_width*uv_height, 1, fp);
	fwrite(data_v, uv_width*uv_height, 1, fp);

	fclose(fp);
	free(data_y);
	free(data_u);
	free(data_v);

	return 0;
}

/**
 * Generate RGB24 colorbar.
 * @param width    Width of Output RGB file.
 * @param height   Height of Output RGB file.
 * @param url_out  Location of Output RGB file.
 */
int simplest_rgb24_colorbar(int width, int height,char *url_out)
{
    if(url_out == NULL)
        return -1;

	unsigned char *data = NULL;
	int barwidth;
	char filename[100] = {0};
	FILE *fp = NULL;
	int i = 0,j = 0;
 
	data = (unsigned char *)malloc(width*height*3);
	barwidth = width/8;
 
	if((fp = fopen(url_out,"wb+")) == NULL){
		printf("Error: Cannot create file!");
		return -1;
	}
 
	for( j=0; j < height; j++){
		for( i=0; i < width; i++){
			int barnum = i/barwidth;
			switch(barnum){
			case 0:
				data[(j*width+i)*3+0] = 255;
				data[(j*width+i)*3+1] = 255;
				data[(j*width+i)*3+2] = 255;
				break;				   
			case 1:
				data[(j*width+i)*3+0] = 255;
				data[(j*width+i)*3+1] = 255;
				data[(j*width+i)*3+2] = 0;
				break;			   
			case 2:
				data[(j*width+i)*3+0] = 0;
				data[(j*width+i)*3+1] = 255;
				data[(j*width+i)*3+2] = 255;
				break;
			case 3:
				data[(j*width+i)*3+0] = 0;
				data[(j*width+i)*3+1] = 255;
				data[(j*width+i)*3+2] = 0;
				break;
			case 4:
				data[(j*width+i)*3+0] = 255;
				data[(j*width+i)*3+1] = 0;
				data[(j*width+i)*3+2] = 255;
				break;
			case 5:
				data[(j*width+i)*3+0] = 255;
				data[(j*width+i)*3+1] = 0;
				data[(j*width+i)*3+2] = 0;
				break;
			case 6:
				data[(j*width+i)*3+0] = 0;
				data[(j*width+i)*3+1] = 0;
				data[(j*width+i)*3+2] = 255;
				break;
			case 7:
				data[(j*width+i)*3+0] = 0;
				data[(j*width+i)*3+1] = 0;
				data[(j*width+i)*3+2] = 0;
				break;
			}
 		}
	}

	fwrite(data, width*height*3 , 1, fp);
	fclose(fp);
	free(data);
 
	return 0;
}

