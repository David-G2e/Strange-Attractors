/***********************************************************************
StrangeAttractors - Program modeling the Lorentz Attractor with birth/death
rate, as well as the option to add particles. Data is exchanged between a
background animation thread and the foreground rendering thread using
a triple buffer, and retained-mode OpenGL rendering using vertex and
index buffers. A ring buffer is utilized to add particles. 

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include <unistd.h>
#include <iostream>
#include <Threads/Thread.h>
#include <Threads/TripleBuffer.h>
#include <Threads/RingBuffer.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Random.h>
#include <Geometry/OrthogonalTransformation.h>
#include <GL/gl.h>
#include <GL/GLGeometryVertex.h>
#include <GL/GLVertexBuffer.h>
#include <vector>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/ToolManager.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>

class StrangeAttractors:public Vrui::Application
	{
	/* Embedded classes: */
	private:
	typedef GLGeometry::Vertex<void,0,GLubyte,4,void,float,3> ParticleVertex; // Type for Particles storing colors and positions
	typedef std::vector<ParticleVertex> ParticleList; // Vector of particleVertex
	typedef std::vector<float> TimeList; // Vector of times
	typedef GLVertexBuffer<ParticleVertex> VertexBuffer; // Type for OpenGL buffers holding mesh vertices
	class SeedParticlesTool; // Forward declaration
	typedef Vrui::GenericToolFactory<SeedParticlesTool> SeedParticlesToolFactory; // Tool class uses the generic factory class	
	
	/* Elements: */
	private:
	int initParticleSize; // number of Particles
	float timeDecay; // lifespan of a Particle
	struct ParticleTimeList{
		ParticleList particleList;
		TimeList timeList;
	};
	Threads::TripleBuffer<ParticleTimeList> particleVertices;
	Threads::RingBuffer<ParticleVertex> inputParticles;
	const ParticleList* oldParticleList; //pointer to array of particles from the last step
	const TimeList* oldTimeList; //pointer to array of time from the last step
	VertexBuffer vertexBuffer; // Buffer holding mesh vertices
	Threads::Thread strangeAttractorsThread; // Thread object for the background StrangeAttractors thread
	
	class SeedParticlesTool:public Vrui::Tool,public Vrui::Application::Tool<StrangeAttractors>// The custom tool class, derived from application tool class
		{
		friend class Vrui::GenericToolFactory<SeedParticlesTool>;
		
		/* Elements: */
		private:
		static SeedParticlesToolFactory* factory; // Pointer to the factory object for this class
		
		/* Constructors and destructors: */
		public:
		static void initClass(void); // Initializes the custom tool's factory class
		SeedParticlesTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
		
		/* Methods: */
		virtual const Vrui::ToolFactory* getFactory(void) const;
		virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
		virtual void frame(void);
		};
	
	/* Private methods: */
	void updateMesh(ParticleTimeList& thisParticleTimeList); // Recalculates all ParticleTimeList
	void* strangeAttractorsThreadMethod(void); // Thread method for the background StrangeAttractors thread
	/* Constructors and destructors: */
	public:
	StrangeAttractors(int& argc,char**& argv);
	virtual ~StrangeAttractors(void);
	
	/* Methods from Vrui::Application: */
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	};

/**********************************
Methods of class StrangeAttractors:
**********************************/

void StrangeAttractors::updateMesh(StrangeAttractors::ParticleTimeList& thisParticleTimeList)
	{
	/* Update the [x,y,z] coordinate of all Particles: */
	thisParticleTimeList.particleList.clear();
	thisParticleTimeList.timeList.clear();
	float s = 10;
	float r = 28;
	float b = 2.667;

	TimeList::const_iterator oldTimeListIt = oldTimeList->begin();
	ParticleList::const_iterator oldParticleListIt = oldParticleList->begin();
	for(;oldParticleListIt!=oldParticleList->end();++oldParticleListIt,++oldTimeListIt)
		{
		float tp = *oldTimeListIt;
		if (tp>Vrui::getApplicationTime())
			{
			/* initial x,y,z coordinates */
			ParticleVertex pv = *oldParticleListIt;
			//std::cout<< pv.position[1] << std::endl;
			float x = pv.position[0];
			float y = pv.position[1];
			float z = pv.position[2];
			
			/* change at each time step for the Lorenz attractor system: */
			ParticleVertex::Position::Vector dot;
			dot[0]=s*(y-x);
			dot[1]=x*(r-z)-y;
			dot[2]=(x*y)-(b*z);
			float t = 0.003;
			
			/* updated color */
			if (pv.color[1]>0)
				{
				pv.color[1] -= .1;
				}
			else if(pv.color[0]>0)
				{
				pv.color[0] -= .1;
				}
			else
				{
				pv.color[2] -= .1;
				}
			
			/* updated positions: */
			pv.position+= dot*t;
		
			thisParticleTimeList.particleList.push_back (pv);
			thisParticleTimeList.timeList.push_back (tp);
			}
		}
	while(!inputParticles.empty())
		{
		thisParticleTimeList.particleList.push_back(inputParticles.read());
		thisParticleTimeList.timeList.push_back(Vrui::getApplicationTime()+timeDecay);
		}
	//work from step for new iteration
	oldParticleList = &thisParticleTimeList.particleList;
	oldTimeList = &thisParticleTimeList.timeList;
	}

void* StrangeAttractors::strangeAttractorsThreadMethod(void)
	{
	while(true)
		{
		/* Sleep for approx. 1/60th of a second: */
		usleep(1000000/60);
		
		/* Start a new value in the mesh triple buffer: */
		ParticleTimeList& thisParticleTimeList = particleVertices.startNewValue();
		
		/* Recalculate the mesh vertices in the new triple buffer slot: */
		updateMesh(thisParticleTimeList);

		/* Push the new triple buffer slot to the foreground thread: */
		particleVertices.postNewValue();
		
		/* Wake up the foreground thread by requesting a Vrui frame immediately: */
		Vrui::requestUpdate();
		}
	return 0;
	}

StrangeAttractors::StrangeAttractors(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	initParticleSize(100), 
	inputParticles(100),
	timeDecay(10)
	{
	SeedParticlesTool::initClass();
	
	ParticleTimeList& thisParticleTimeList = particleVertices.startNewValue();
	for(int i = 0; i< initParticleSize ;++i)
		{
		/* Initialize the random position of Particles: */
		ParticleVertex pv;
		
		pv.position[0]=Math::randUniformCO(-20.0f,20.0f);
		pv.position[1]=Math::randUniformCO(-20.0f,20.0f);
		pv.position[2]=Math::randUniformCO(-20.0f,20.0f);
		
		/* Initialize the random color of Particles: */
		pv.color[0]=Math::randUniformCO(64,256);
		pv.color[1]=Math::randUniformCO(64,256);
		pv.color[2]=Math::randUniformCO(64,256);
		pv.color[3]=255;
		
		thisParticleTimeList.particleList.push_back(pv);
		
		/* Initialize the time of Particles: */
		thisParticleTimeList.timeList.push_back(Vrui::getApplicationTime()+timeDecay);
		}
	oldParticleList = &thisParticleTimeList.particleList;
	oldTimeList = &thisParticleTimeList.timeList;
	
	/* Calculate the first full mesh state in a new triple buffer slot: */
	particleVertices.postNewValue();
	
	/* Start the background StrangeAttractors thread: */
	strangeAttractorsThread.start(this,&StrangeAttractors::strangeAttractorsThreadMethod);
	}

StrangeAttractors::~StrangeAttractors(void)
	{
	/* Shut down the background StrangeAttractors thread: */
	strangeAttractorsThread.cancel();
	strangeAttractorsThread.join();
	}

void StrangeAttractors::frame(void)
	{
	/* Check if there is a new entry in the triple buffer and lock it: */
	if(particleVertices.lockNewValue())
		{
		const ParticleTimeList& thisParticleList=particleVertices.getLockedValue();
				
		/* Point the vertex buffer to the new mesh vertices: */
		vertexBuffer.setSource(thisParticleList.particleList.size(),thisParticleList.particleList.data());
		}
	}

void StrangeAttractors::display(GLContextData& contextData) const
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

void StrangeAttractors::resetNavigation(void)
	{
	/* Center and scale the object: */
	Vrui::setNavigationTransformation(Vrui::Point::origin,150.0);
	}

/********************************************************************
Static elements of class StrangeAttractors::SeedParticlesToolFactory:
********************************************************************/

StrangeAttractors::SeedParticlesToolFactory* StrangeAttractors::SeedParticlesTool::factory=0;

void StrangeAttractors::SeedParticlesTool::initClass(void)
	{
	/* Create a factory object for the custom tool class: */
	factory=new SeedParticlesToolFactory("SeedParticlesTool","Seed Particles",0,*Vrui::getToolManager());
	
	/* Set the custom tool class' input layout: */
	factory->setNumButtons(1);
	factory->setButtonFunction(0,"Seed Particles");
	
	/* Register the custom tool class with Vrui's tool manager: */
	Vrui::getToolManager()->addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	}

StrangeAttractors::SeedParticlesTool::SeedParticlesTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::Tool(factory,inputAssignment)
	{
	}

const Vrui::ToolFactory* StrangeAttractors::SeedParticlesTool::getFactory(void) const
	{
	return factory;
	}

void StrangeAttractors::SeedParticlesTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState) // Button has just been pressed
		{
		std::cout<<"MyTool: Button "<<buttonSlotIndex<<" has just been pressed"<<std::endl;
		}
	else // Button has just been released
		{
		std::cout<<"MyTool: Button "<<buttonSlotIndex<<" has just been released"<<std::endl;
		}
	}
void StrangeAttractors::SeedParticlesTool::frame(void)
	{
		if(getButtonState(0))
		{
			ParticleVertex pv;
			pv.position = Vrui::getNavigationTransformation().inverseTransform(getButtonDevicePosition(0));
			
			pv.position[0]+=Math::randUniformCO(-3.0f,3.0f);
			pv.position[1]+=Math::randUniformCO(-3.0f,3.0f);
			pv.position[2]+=Math::randUniformCO(-3.0f,3.0f);
			
			pv.color[0]=Math::randUniformCO(32,256);
			pv.color[1]=Math::randUniformCO(32,256);
			pv.color[2]=Math::randUniformCO(32,256);
			pv.color[3]=255;
			
			application->inputParticles.write(pv);
			Vrui::scheduleUpdate(Vrui::getNextAnimationTime());
		}
	}
	
VRUI_APPLICATION_RUN(StrangeAttractors)
