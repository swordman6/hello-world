#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parseh264.h"

/*****************************************************************************************************
 * H.264/AVC 视频编码标准: 
 * NAL(network abstraction layer) = VCL(video coding layer) + header
 * 
 * H264 码流数据由 NALU 序列组成，相邻的 NALU 由起始码 StartCode 隔开
 * 起始码 StartCode 的两种形式：3 字节的 0x000001 和 4 字节的 0x00000001。
 * 
 * NALU Header 由 3 部分组成，共占用 1 个字节
 * forbidden_bit(1bit)      禁止位,默认为0，为1时被丢弃
 * nal_reference_bit(2bit)  重要性指示位，0-3，3最高
 * nal_unit_type(5bit)      类型位，0-12为H264定义,12-23保留,24-31未使用
 * 
 * nal_unit_type: 
 * 0  未使用
 * 1  不分区，非IDR的slice
 * 2  使用Data Partitioning,且为Slice A
 * 3  使用Data Partitioning,且为Slice B
 * 4  使用Data Partitioning,且为Slice C
 * 5  IDR图像中的slice
 * 6  补充增强信息单元(SEI)
 * 7  序列参数集(Sequence Parameter Set, SPS)
 * 8  图像参数集(Picture  Parametet Set, PPS)
 * 9  分界符
 * 10 序列结束
 * 11 码流结束
 * 12 填充
 * 
 * NALU 的主体部分: NALU = NALU Header + EBSP
 * SODB: String Of Data Bits 原始数据比特流，编码后的原始数据，即VCL数据。
 * RBSP: 原始字节序列载荷。由于 SODB 长度不一定是 8 的倍数，添加8字节对齐
 * EBSP: 扩展字节序列载荷。RBSP 数据中每遇到两个字节连续为 0，就插入一个字节的 0x03，这样就得到了 EBSP。
 *
 ********************************************************************************************/
 
typedef enum {
	NALU_TYPE_SLICE     = 1,
	NALU_TYPE_DPA       = 2,
	NALU_TYPE_DPB       = 3,
	NALU_TYPE_DPC       = 4,
	NALU_TYPE_IDR       = 5,
	NALU_TYPE_SEI       = 6,
	NALU_TYPE_SPS       = 7,
	NALU_TYPE_PPS       = 8,
	NALU_TYPE_AUD       = 9,
	NALU_TYPE_EOFSEQ    = 10,
	NALU_TYPE_EOFSTREAM = 11,
	NALU_TYPE_FILL      = 12,
} NaluType;
 
typedef enum {
	NALU_PRIORITY_DISPOSABLE = 0, //B帧
	NALU_PRIORITY_LOW        = 1, //P帧
	NALU_PRIORITY_HIGH       = 2, //I帧
	NALU_PRIORITY_HIGHEST    = 3  //IDR帧,SPS,PPS
} NaluPriority;

typedef struct {
	long len;                      //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
	long max_size;                 //! Nal Unit Buffer size
	int forbidden_bit;             //! should be always FALSE
	int nal_reference_idc;         //! NALU_PRIORITY_xxxx
	int nal_unit_type;             //! NALU_TYPE_xxxx    
	char *buf;                     //! contains the first byte followed by the EBSP
} NALU_t;

static int FindStartCode2 (unsigned char *Buf){
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=1) //0x000001?
        return 0; 
	else 
        return 1;
}
 
static int FindStartCode3 (unsigned char *Buf){
	if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=0 || Buf[3] !=1) //0x00000001?
        return 0;
	else 
        return 1;
}

long GetH264Header(FILE *fp, int *codeprefix_len, NALU_t *nalu_st)
{
    unsigned char header[4];
    long offset;

    if(3 != fread(header, 1, 3, fp))
        return -1;

    while(!feof(fp))
    {
        if(!FindStartCode2(header))
        {
            if(1 != fread(header+3, 1, 1, fp))
                return -1;

            if(!FindStartCode3(header))
            {              
                header[0] = header[1];
                header[1] = header[2];
                header[2] = header[3];
            }
            else
            {
                *codeprefix_len = 4;
                break;
            }
        }
        else
        {
            *codeprefix_len = 3;
            break;
        }

    }

    offset = ftell(fp);

    return offset;
}

int GetAnnexbNALU(FILE *fp, NALU_t *nalu_st)
{
    if(fp == NULL || nalu_st == NULL)
        return -1;

    long startoffset,endoffset;
    int  startcodeprefix_len,endcodeprefix_len;

    startoffset = GetH264Header(fp, &startcodeprefix_len, nalu_st);
    if(startoffset < 0)
        return -1;

    endoffset   = GetH264Header(fp, &endcodeprefix_len, nalu_st);
    if(endoffset < 0)
        return -1;

    nalu_st->len = endoffset - startoffset - endcodeprefix_len;

    fseek(fp, startoffset, SEEK_SET);

    if(nalu_st->len != fread(nalu_st->buf, 1, nalu_st->len, fp))
        return -1;

    nalu_st->forbidden_bit = nalu_st->buf[0] & 0x80; //1 bit
	nalu_st->nal_reference_idc = nalu_st->buf[0] & 0x60; // 2 bit
	nalu_st->nal_unit_type = (nalu_st->buf[0]) & 0x1f;// 5 bit

    return nalu_st->len;
}

int simplest_h264_parser(char *url, long max_nalu)
{
    if(url == NULL)
        return -1;

    NALU_t nalu_st;

    FILE *fp = NULL;
    fp = fopen(url, "r");
    if(fp == NULL){
        perror("fopen:");
        return -1;
    }

    nalu_st.max_size = max_nalu;
    nalu_st.buf = (char *)malloc(max_nalu);
    if(nalu_st.buf == NULL){
        perror("malloc:");
        fclose(fp);
        return -1;
    }

    printf("-----+-------- NALU Table ------+---------+\n");
	printf(" NUM |    POS  |    IDC |  TYPE |   LEN   |\n");
	printf("-----+---------+--------+-------+---------+\n");

    int data_offset=0;
	int nal_num=0;

    while(!feof(fp)) 
	{
		int data_lenth;
		data_lenth=GetAnnexbNALU(fp, &nalu_st);
        if(data_lenth < 0){
            free(nalu_st.buf);
            fclose(fp);
            return -1;
        }
   
		char type_str[20]={0};
		switch(nalu_st.nal_unit_type){
			case NALU_TYPE_SLICE:sprintf(type_str,"SLICE");break;
			case NALU_TYPE_DPA:sprintf(type_str,"DPA");break;
			case NALU_TYPE_DPB:sprintf(type_str,"DPB");break;
			case NALU_TYPE_DPC:sprintf(type_str,"DPC");break;
			case NALU_TYPE_IDR:sprintf(type_str,"IDR");break;
			case NALU_TYPE_SEI:sprintf(type_str,"SEI");break;
			case NALU_TYPE_SPS:sprintf(type_str,"SPS");break;
			case NALU_TYPE_PPS:sprintf(type_str,"PPS");break;
			case NALU_TYPE_AUD:sprintf(type_str,"AUD");break;
			case NALU_TYPE_EOFSEQ:sprintf(type_str,"EOSEQ");break;
			case NALU_TYPE_EOFSTREAM:sprintf(type_str,"EOSTREAM");break;
			case NALU_TYPE_FILL:sprintf(type_str,"FILL");break;
		}
		char idc_str[20]={0};
		switch(nalu_st.nal_reference_idc>>5){
			case NALU_PRIORITY_DISPOSABLE:sprintf(idc_str,"DISPOS");break;
			case NALU_PRIORITY_LOW:sprintf(idc_str,"LOW");break;
			case NALU_PRIORITY_HIGH:sprintf(idc_str,"HIGH");break;
			case NALU_PRIORITY_HIGHEST:sprintf(idc_str,"HIGHEST");break;
		}
 
		fprintf(stdout,"%5d| %8d| %7s| %6s| %8ld|\n",nal_num,data_offset,idc_str,type_str,nalu_st.len);
 
		data_offset=data_offset+data_lenth;
 
		nal_num++;
	}

    free(nalu_st.buf);
    fclose(fp);

    return 0;
}

