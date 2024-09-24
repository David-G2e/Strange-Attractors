/***********************************************************************
StrangeAttractors1 - Program modeling the Lorentz Attractor with 1000 particles.
Data is exchanged between a background animation thread and the foreground 
rendering thread using a triple buffer, and retained-mode OpenGL rendering
using vertex and index buffers. Data is stored and accessed using pointers. 

Last Modified: 8/20/24
***********************************************************************/

#include <unistd.h>
#include <iostream>
#include <Threads/Thread.h>
#include <Threads/TripleBuffer.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Random.h>
#include <GL/gl.h>
#include <GL/GLGeometryVertex.h>
#include <GL/GLVertexBuffer.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

class Animation:public Vrui::Application
	{
	/* Embedded classes: */
	private:
	typedef GLGeometry::Vertex<void,0,GLubyte,4,void,float,3> ParticleVertex; // Type for Particles storing colors and positions
	typedef GLVertexBuffer<ParticleVertex> VertexBuffer; // Type for OpenGL buffers holding mesh vertices
	
	/* Elements: */
	private:
	int particleSize; // number of Particles
	Threads::TripleBuffer<ParticleVertex*> particleVertices;
	const ParticleVertex* oldParticles; //array of particles from the last step
	VertexBuffer vertexBuffer; // Buffer holding mesh vertices
	Threads::Thread animationThread; // Thread object for the background animation thread
	
	/* Private methods: */
	void updateMesh(ParticleVertex* pv); // Recalculates all ParticleVertex
	void* animationThreadMethod(void); // Thread method for the background animation thread
	/* Constructors and destructors: */
	public:
	Animation(int& argc,char**& argv);
	virtual ~Animation(void);
	
	/* Methods from Vrui::Application: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	};

/**************************
Methods of class Animation:
**************************/

void Animation::updateMesh(Animation::ParticleVertex* pv)
	{
	/* Update the [x,y,z] coordinate of all Particles: */
	ParticleVertex* pPtr=pv;
	ParticleVertex* pvEnd=pv+particleSize;
	const ParticleVertex* oldPtr = oldParticles;
	//std::cout << pPtr;
	//std::cout << " ";	
	/* System parameters: */
	float s = 10;
	float r = 28;
	float b = 2.667;
	
	for(ParticleVertex* pPtr=pv;pPtr!=pvEnd;++pPtr,++oldPtr)
		{
		/* initial x,y,z coordinates */
		float x = oldPtr->position[0];
		float y = oldPtr->position[1];
		float z = oldPtr->position[2];
		
		/* change at each time step for the Lorenz attractor system: */
		float x_dot = s*(y-x);
		float y_dot = x*(r-z)-y;
		float z_dot = (x*y)-(b*z);
		float t = 0.003;
		
		/* updated positions: */
		pPtr->position[0]= oldPtr->position[0] + t*x_dot;
		//std::cout << x;
		//std::cout << " ";
		pPtr->position[1]= oldPtr->position[1] + t*y_dot;
		pPtr->position[2]= oldPtr->position[2] + t*z_dot;
		
		/*updated color: */
		pPtr->color[0]= oldPtr->color[0];
		pPtr->color[1]= oldPtr->color[1];
		pPtr->color[2]= oldPtr->color[2];
		pPtr->color[3]= oldPtr->color[3];
		}
	
	//work from step for new iteration
	oldParticles = pv;
	}

void* Animation::animationThreadMethod(void)
	{
	while(true)
		{
		/* Sleep for approx. 1/60th of a second: */
		usleep(1000000/60);
		
		/* Start a new value in the mesh triple buffer: */
		ParticleVertex* pv=particleVertices.startNewValue();

		/* Recalculate the mesh vertices in the new triple buffer slot: */
		updateMesh(pv);
		
		/* Push the new triple buffer slot to the foreground thread: */
		particleVertices.postNewValue();
		
		/* Wake up the foreground thread by requesting a Vrui frame immediately: */
		Vrui::requestUpdate();
		}
	
	return 0;
	}

Animation::Animation(int& argc,char**& argv)
	:Vrui::Application(argc,argv)
	{
	/* Initialize the number of Particles: */
	particleSize=10000;
		
	for(int i=0;i<3;++i)
		{
		/* Access the i-th triple buffer slot: */
		ParticleVertex*& pv=particleVertices.getBuffer(i);

		/* Allocate the in-memory vertex array: */
		pv=new ParticleVertex[particleSize];
		}

	ParticleVertex* pv=particleVertices.startNewValue();
	oldParticles = pv;
	ParticleVertex* pvEnd=pv+particleSize;
	for(ParticleVertex* pPtr=pv;pPtr!=pvEnd;++pPtr)
		{
		/* Initialize the random position of Particles: */
		pPtr->position[0]=Math::randUniformCO(-20.0f,20.0f);
		pPtr->position[1]=Math::randUniformCO(-20.0f,20.0f);
		pPtr->position[2]=Math::randUniformCO(-20.0f,20.0f);
		
		/* Initialize the random color of Particles: */
		pPtr->color[0]=Math::randUniformCO(32,256);
		pPtr->color[1]=Math::randUniformCO(32,256);
		pPtr->color[2]=Math::randUniformCO(32,256);
		pPtr->color[3]=255;
		}
	
	/* Calculate the first full mesh state in a new triple buffer slot: */
	particleVertices.postNewValue();
	
	/* Start the background animation thread: */
	animationThread.start(this,&Animation::animationThreadMethod);
	}

Animation::~Animation(void)
	{
	/* Shut down the background animation thread: */
	animationThread.cancel();
	animationThread.join();
	
	/* Delete the in-memory vertex arrays in all three triple buffer slots: */
	for(int i=0;i<3;++i)
		delete[] particleVertices.getBuffer(i);
	}

	
void Animation::frame(void)
	{
	/* Check if there is a new entry in the triple buffer and lock it: */
	if(particleVertices.lockNewValue())
		{
		/* Point the vertex buffer to the new mesh vertices: */
		vertexBuffer.setSource(particleSize,particleVertices.getLockedValue());
		}
	}

void Animation::display(GLContextData& contextData) const
	{
	/* Save OpenGL state: */
	glPushAttrib(GL_ENABLE_BIT|GL_POINT_BIT);
	
	glDisable(GL_LIGHTING);
	glPointSize(3.0f);
	
	/* Bind the vertex buffer (which automatically uploads any new vertex data): */
	VertexBuffer::DataItem* vbdi=vertexBuffer.bind(contextData);
		
	vertexBuffer.draw(GL_POINTS, vbdi);
	
	/* Unbind the buffers: */
	vertexBuffer.unbind();
	
	/* Restore OpenGL state: */
	glPopAttrib();
	}

void Animation::resetNavigation(void)
	{
	/* Center and scale the object: */
	Vrui::setNavigationTransformation(Vrui::Point::origin,150.0);
	}

VRUI_APPLICATION_RUN(Animation)
