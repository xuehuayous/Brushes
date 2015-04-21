
///  \file CZUtil.cpp
///  \brief This is the file implement a series of utilities.
///
///		(description)
///
///  \version	1.0.0
///	 \author	Charly Zhang<chicboi@hotmail.com>
///  \date		2014-09-11
///  \note

#include "CZUtil.h"
#include "path/CZBezierSegment.h"
#include "basic/CZ3DPoint.h"
#include "CZDefine.h"
#include "graphic/glDef.h"

using namespace std;

/// ��ͬ��ɫģʽ��ת��
void CZUtil::HSVtoRGB(float h, float s, float v, float &r, float &g, float &b)
{
	if (s == 0) 
	{
		r = g = b = v;
	} 
	else 
	{
		float   f,p,q,t;
		int     i;

		h *= 360;

		if (h == 360.0f) 
		{
			h = 0.0f;
		}

		h /= 60;
		i = floor(h);

		f = h - i;
		p = v * (1.0 - s);
		q = v * (1.0 - (s*f));
		t = v * (1.0 - (s * (1.0 - f)));

		switch (i) {
		case 0: r = v; g = t; b = p; break;
		case 1: r = q; g = v; b = p; break;
		case 2: r = p; g = v; b = t; break;
		case 3: r = p; g = q; b = v; break;
		case 4: r = t; g = p; b = v; break;
		case 5: r = v; g = p; b = q; break;
		}
	}
}   
void CZUtil::RGBtoHSV(float r, float g, float b, float &h, float &s, float &v)
{
	float max = Max(r, Max(g, b));
	float min = Min(r, Min(g, b));
	float delta = max - min;

	v = max;
	s = (max != 0.0f) ? (delta / max) : 0.0f;

	if (s == 0.0f) 
	{
		h = 0.0f;
	} 
	else
	{
		if (r == max) {
			h = (g - b) / delta;
		} else if (g == max) {
			h = 2.0f + (b - r) / delta;
		} else if (b == max) {
			h = 4.0f + (r - g) / delta;
		}

		h *= 60.0f;

		if (h < 0.0f) 
		{
			h += 360.0f;
		}
	}

	h /= 360.0f;
}

/// ��UUID
char* CZUtil::generateUUID()
{
	char *buf = new char[37];

	const char *c = "89ab";
	char *p = buf;
	int n;
	for( n = 0; n < 16; ++n )
	{
		int b = rand()%255;
		switch( n )
		{
		case 6:
			sprintf(p, "4%x", b%15 );
			break;
		case 8:
			sprintf(p, "%c%x", c[rand()%strlen(c)], b%15 );
			break;
		default:
			sprintf(p, "%02x", b);
			break;
		}

		p += 2;
		switch( n )
		{
		case 3:
		case 5:
		case 7:
		case 9:
			*p++ = '-';
			break;
		}
	}
	*p = 0;
	return buf;
}

/// ��һ��������ɢ�����ڽ�������α�������������
/// 
///		������㣨nodes���γ�һ�����α��������ߣ��ٽ����ߴ�ɢ�����ɸ����Ƶ㣨points��
/// 
///		/param nodes		- ��������ά���
///		/param points		- ��ɢ��õ��Ļ��Ƶ�����
///		/return				- ��ɢ��õ��Ļ��Ƶ���Ŀ
unsigned int CZUtil::flattenNodes2Points(const vector<CZBezierNode> &nodes, bool closed, vector<CZ3DPoint> &points)
{
	int numNodes = nodes.size();

	points.clear();

	if (numNodes == 1)
	{
		CZBezierNode lonelyNode = nodes.back();
		points.push_back(lonelyNode.anchorPoint);
		return 1;
	}

	int numSegs = closed ? numNodes : numNodes - 1;

	CZBezierSegment   *segment = NULL;
	for (int i = 0; i < numSegs; i++) 
	{
		CZBezierNode a = nodes[i];
		CZBezierNode b = nodes[(i+1) % numNodes];

		segment = CZBezierSegment::segmentBetweenNodes(a,b);
		segment->flattenIntoArray(points);
		delete segment;
	}

	return points.size();
}

/// �ж��Ƿ�֧�������ɫ
bool CZUtil::canUseHDTexture()
{
	return true;
}

void CZUtil::CZCheckGLError_(const char *file, int line)
{
	int    retCode = 0;
	GLenum glErr = glGetError();

	while (glErr != GL_NO_ERROR) 
	{
        
#if USE_OPENGL
        const GLubyte* sError = gluErrorString(glErr);
        
        if (sError)
            LOG_INFO("GL Error #%d (%s) in File %s at line: %d\n",glErr,gluErrorString(glErr),file,line);
        else
            LOG_INFO("GL Error #%d (no message available) in File %s at line: %d\n",glErr,file,line);
        
#elif USE_OPENGL_ES
        switch (glErr) {
            case GL_INVALID_ENUM:
                LOG_ERROR("GL Error: Enum argument is out of range\n");
                break;
            case GL_INVALID_VALUE:
                LOG_ERROR("GL Error: Numeric value is out of range\n");
                break;
            case GL_INVALID_OPERATION:
                LOG_ERROR("GL Error: Operation illegal in current state\n");
                break;
                //        case GL_STACK_OVERFLOW:
                //            NSLog(@"GL Error: Command would cause a stack overflow");
                //            break;
                //        case GL_STACK_UNDERFLOW:
                //            NSLog(@"GL Error: Command would cause a stack underflow");
                //            break;
            case GL_OUT_OF_MEMORY:
                LOG_ERROR("GL Error: Not enough memory to execute command\n");
                break;
            case GL_NO_ERROR:
                if (1) {
                    LOG_ERROR("No GL Error\n");
                }
                break;
            default:
                LOG_ERROR("Unknown GL Error\n");
                break;
        }
#endif
        
		retCode = 1;
		glErr = glGetError();
	}
	//return retCode;
};

/// ���Һ���,��[0,1]��[0,1] -CZFreehandTool������
float CZUtil::sineCurve(float input)
{
	float result;

	input *= M_PI; // move from [0.0, 1.0] tp [0.0, Pi]
	input -= M_PI_2; // shift back onto a trough

	result = sin(input) + 1; // add 1 to put in range [0.0,2.0]
	result /= 2; // back to [0.0, 1.0];

	return result;
}


void CZUtil::checkPixels(int w_, int h_)
{
	PixDataType *pix = new PixDataType[w_*h_*4];

	glReadPixels(0,0,w_,h_,GL_RGBA, GL_PIXEL_TYPE,pix);

	bool over = false;
    long num = 0;
	for(int i=0; i<h_; i++)
	{
		for(int j=0; j<w_; j++)
		{
			int ind = i*w_+j;
			if( pix[4*ind+0] != 0 ||
				pix[4*ind+1] != 0 ||
				pix[4*ind+2] != 0 ||
                pix[4*ind+3] != 0)
            {
#if USE_OPENGL
				LOG_INFO("(%d,%d):\t%f\t%f\t%f\t%f\n",i,j,pix[4*ind+0],pix[4*ind+1],pix[4*ind+2],pix[4*ind+3]);
#elif USE_OPENGL_ES
                LOG_INFO("(%d,%d):\t%d\t%d\t%d\t%d\n",i,j,pix[4*ind+0],pix[4*ind+1],pix[4*ind+2],pix[4*ind+3]);
#endif
                num ++;
            }
            //over =true;
			//break;
		}
		if(over) break;
	}

	LOG_INFO("finished!, total number of satisfied is %ld\n",num);

	delete [] pix;
}

/// ���ƾ���
void CZUtil::drawRect(const CZRect &rect, const CZAffineTransform &transform)
{
	CZ2DPoint corners[4];
	GLuint  vbo = 0;

	corners[0] = rect.origin;
	corners[1] = CZ2DPoint(rect.getMaxX(), rect.getMinY());
	corners[2] = CZ2DPoint(rect.getMaxX(), rect.getMaxY());
	corners[3] = CZ2DPoint(rect.getMinX(), rect.getMaxY());

	for (int i = 0; i < 4; i++) 
	{
		corners[i] = transform.applyTo2DPoint(corners[i]);
	}

	const GLfloat quadVertices[] = 
	{
		corners[0].x, corners[0].y, 0.0, 0.0,
		corners[1].x, corners[1].y, 1.0, 0.0,
		corners[3].x, corners[3].y, 0.0, 1.0,
		corners[2].x, corners[2].y, 1.0, 1.0,
	};

	// create, bind, and populate VBO
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, quadVertices, GL_STATIC_DRAW);

	// set up attrib pointers
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (void*)8);
	glEnableVertexAttribArray(1);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDeleteBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

