#define setPixel(p,x,y,c) \
	do { \
	p->data[0][x * 3 + y*p->linesize[0]] = ((uint8_t)(c>>16)); \
	p->data[0][x * 3 + y*p->linesize[0] + 1] = ((uint8_t)(c>>8)); \
	p->data[0][x * 3 + y*p->linesize[0] + 2] = (uint8_t)c; \
	} while(0)

