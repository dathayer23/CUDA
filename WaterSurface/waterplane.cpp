#include "waterplane.h"
#include "wavemap.h"
#include "vector.h"
#include <cstdlib>
#include <iostream> 
#include <math.h>

WaterPlane* WaterPlane::WaterPlaneExemplar = 0;

WaterPlane::WaterPlane(){
	showEdges=false;
	vertices = new Vector[1];
	normals = new Vector[1];

}

// Singleton
WaterPlane* WaterPlane::getWaterPlane(){

	if(WaterPlaneExemplar == 0){
		WaterPlaneExemplar = new WaterPlane;
	}
	return WaterPlaneExemplar;
}

void WaterPlane::toggleEdges(){

	showEdges=!showEdges;
}


void WaterPlane::configure(Vector upperLeft, Vector lowerRight, float dampFactor, float resolution)
{

	stepSize = 1.0f/resolution;
	resolutionFactor = resolution;
	sizeX = (int) abs(upperLeft.z - lowerRight.z);
	sizeY = (int) abs(upperLeft.x - lowerRight.x);
	pointsX = (int)(sizeX * resolution);
	pointsY = (int)(sizeY * resolution);
	uLeft = upperLeft;
	lRight = lowerRight;
	baseHeight = lRight.y;
	waveMap = new WaveMap(pointsX, pointsY, dampFactor);
	initBuffer();
	drawMesh();
}


void WaterPlane::createVBO(GLuint* vbo, int size)
{
	// create buffer object
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, size, 0, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

}


// create index buffer for rendering quad mesh
void WaterPlane::createMeshIndexBuffer(GLuint *id, int w, int h)
{
	int size = ((w*2)+2)*(h-1)*sizeof(GLuint);

	// create index buffer
	glGenBuffersARB(1, id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *id);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER, size, 0, GL_STATIC_DRAW);

	// fill with indices for rendering mesh as triangle strips
	GLuint *indices = (GLuint *) glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	if (!indices) {
		return;
	}

	for(int y=0; y<h-1; y++) {
		for(int x=0; x<w; x++) {
			*indices++ = y*w+x;
			*indices++ = (y+1)*w+x;
		}
		// start new strip with degenerate triangle
		*indices++ = (y+1)*w+(w-1);
		*indices++ = (y+1)*w;
	}

	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}



void WaterPlane::initBuffer()
{
	//start and end coordinates for the x directions
	float startX = this->uLeft.x;
	float endX = this->lRight.x;

	//start and end coordinates for the z directions
	float startY = this->uLeft.z;
	float endY = this->lRight.z;

	vertices = new Vector[pointsY*pointsX];
	normals = new Vector[pointsY*pointsX];
	unsigned int count = 0;
	for (float px = startX ; px< endX ; px+=stepSize){
		for (float py = startY; py < endY; py+=stepSize){
			vertices[count].x = px;
			vertices[count].y = 0;
			vertices[count].z = py;
			normals[count].x = 0;
			normals[count].y = 1;
			normals[count].z = 0;
			count++;
		}
	}

	//create vertex buffer for normals and vertices
	createVBO(&vertexBuffer, pointsX*pointsY*sizeof(float)*3);
	createVBO(&normalBuffer, pointsX*pointsY*sizeof(float)*3);
	//create index buffer to draw vertices and normals
	createMeshIndexBuffer(&indexVertexBuffer, pointsX, pointsY);
	createMeshIndexBuffer(&indexNormalBuffer, pointsX, pointsY);

	//bind buffer to data
	glBindBuffer(GL_ARRAY_BUFFER, *&vertexBuffer);
	glBufferData( GL_ARRAY_BUFFER, pointsY*pointsX*3*sizeof(float), vertices, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, *&normalBuffer);
	glBufferData( GL_ARRAY_BUFFER, pointsY*pointsX*3*sizeof(float), normals, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void WaterPlane::disturb(Vector disturbingPoint)
{
	float value = 0.5;

	float y = disturbingPoint.x;
	float x = disturbingPoint.z;
	float h = value;

	this->push(x,y,h);
}

void WaterPlane::disturbArea(float xmin, float zmin, float xmax, float zmax, float height)
{
	if ((zmin < this->uLeft.z) || (zmin > this->lRight.z)) return;
	if ((xmin < this->uLeft.x) || (xmin > this->lRight.x)) return;

	float radius = (float)(getWaterPlaneX(xmax)-getWaterPlaneX(xmin))/2.0f;
	if (radius <= 0) radius = 1;
	float centerX = (float)getWaterPlaneX((xmax+xmin)/2.0f);
	float centerZ = (float)getWaterPlaneY((zmax+zmin)/2.0f);
	float r2 = radius * radius;

	unsigned int xminW = getWaterPlaneX(xmin);
	unsigned int zminW = getWaterPlaneY(zmin);
	unsigned int xmaxW = getWaterPlaneX(xmax);
	unsigned int zmaxW = getWaterPlaneY(zmax);

	for(unsigned int x = xminW; x <= xmaxW; x++)
	{
		for (unsigned int y = zminW; y <= zmaxW; y++)
		{
			float insideCircle = ((x-centerX)*(x-centerX))+((y-centerZ)*(y-centerZ))-r2;
			
			if (insideCircle <= 0) waveMap->push((x),(y), (insideCircle/r2)*height);
		} 
	}
}


void WaterPlane::push(float x, float y, float depth)
{
	//check whether point is inside water plane
	if (x > this->uLeft.z && x < this->lRight.z)
	{
		if (y > this->uLeft.x && y < this->lRight.x)
		{
			waveMap->push(getWaterPlaneX(x),getWaterPlaneY(y), depth);
		} else {
			std::cout<<"Point("<<x<<", "<<y<<") is not within waterplane"<<std::endl;
		}
	} else {
		std::cout<<"Point("<<x<<", "<<y<<") is not within waterplane"<<std::endl;
	}
}

int WaterPlane::getWaterPlaneX(float realX)
{
	int px = 0;

	if (realX >= 0){
		px = (int) ((realX * this->resolutionFactor)+0.5f);
	} else {
		px = (int) ((realX * this->resolutionFactor) + this->uLeft.z+0.5f);
	}
	return px;
}


int WaterPlane::getWaterPlaneY(float realY)
{
	int py = 0;

	if (realY >= 0){
		py = (int) (realY * this->resolutionFactor+0.5f);
	} else {
		py = (int) ((realY * this->resolutionFactor) + this->uLeft.x+0.5f);
	}
	return py;
}


void WaterPlane::update()
{
	//update WaveMap
	waveMap->updateWaveMap();

	float n = 0.0;

	int vIndex = 0;

	int nIndex = 0;

	//updates heights
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, vertexBuffer);
	Vector* vertices2 = (Vector*)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

	for (int x = 0; x< pointsX ; x++){

		for (int y = 0; y < pointsY; y++){

			n = waveMap->getHeight(x,y);
			n += this->baseHeight; 
			vIndex = (y * pointsX) + x;
			Vector v = vertices2[vIndex];
			v.y = n;
			vertices2[vIndex] = v;
		}
	}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);

	// updates normals
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, normalBuffer);
	Vector* normals2 = (Vector*)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

	for (int x = 0; x< pointsX ; x++){

		for (int y = 0; y < pointsY; y++){

			
			/*
			   1  2
			*--*--*
			| /| /|
		  6 *--*--* 3
			| /| /|
			*--*--*
			5  4

			*/

			nIndex = (y * pointsX) + x;
			Vector vertex = vertices2[nIndex];
			Vector newNormal = Vector();
			if (x+1 < pointsX){
				Vector v1 = vertices2[nIndex+1];
				Vector s1 = v1 - vertex;
				if ((y-1) >= 0){
					Vector v2 = vertices2[nIndex-pointsX+1];
					Vector s2 = v2 - vertex;
					newNormal = newNormal + s2.crossProduct(s1);
					Vector t = s2.crossProduct(s1);
				}
				
				if (y+1 < pointsY){
					Vector v6 = vertices2[nIndex+pointsX];
					Vector s6 = v6 - vertex;
					newNormal = newNormal + s1.crossProduct(s6);
				}
			}
			if (x-1 >= 0){
				Vector v4 = vertices2[nIndex-1];
				Vector s4 = v4 - vertex;
				if (y+1 < pointsY){
					Vector v5 = vertices2[nIndex+pointsX-1];
					Vector s5 = v5 - vertex;
					newNormal = newNormal + s5.crossProduct(s4);
				}

				if (y-1 >= 0){
					Vector v3 = vertices2[nIndex-pointsX];
					Vector s3 = v3 - vertex;
					newNormal = newNormal + s4.crossProduct(s3);
				}
			}
			normals2[nIndex] = Vector::Normalize(newNormal);
			
		}
		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	}
	
}


void WaterPlane::drawMesh()
{
	glEnable(GL_LIGHTING);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GLfloat mat_color1[] = { 0.2f, 0.6f, 0.9f };
	GLfloat mat_shininess =  110.0f ;
	GLfloat specRefl[] = {1.0f, 1.0f, 1.0f, 1.0f}; // default

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState(GL_NORMAL_ARRAY);

	glBindBuffer( GL_ARRAY_BUFFER, vertexBuffer );
	glVertexPointer( 3, GL_FLOAT, 0, (char *) NULL );

	glBindBuffer( GL_ARRAY_BUFFER, normalBuffer );
	glNormalPointer(GL_FLOAT, 0, (char *) NULL );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexVertexBuffer);

	if (showEdges)
	{
		glDisable(GL_LIGHTING);
		glColor3f(1,1,1);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawElements(GL_TRIANGLE_STRIP, ((pointsX*2)+2)*(pointsX-1), GL_UNSIGNED_INT, 0); 
	}
	glEnable(GL_LIGHTING);
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_color1);
	glMaterialfv(GL_FRONT, GL_SPECULAR, specRefl); 
	glMaterialfv(GL_FRONT, GL_SHININESS, &mat_shininess);
	glDrawElements(GL_TRIANGLE_STRIP, ((pointsX*2)+2)*(pointsX-1), GL_UNSIGNED_INT, 0); 

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// Disable Pointers
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState( GL_VERTEX_ARRAY );// Disable Vertex Arrays

}

void WaterPlane::deleteVBO(GLuint* vbo)
{
	glDeleteBuffers(1, vbo);
	*vbo = 0;
}


//Destructor
WaterPlane::~WaterPlane(void){
	deleteVBO(&vertexBuffer);
	deleteVBO(&indexVertexBuffer);

	delete [] vertices;
	delete [] normals;
	if(waveMap) delete waveMap;
	
}

