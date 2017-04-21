#include <sstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "application/application.h"
#include "common/log.h"

#define XML_FILENAME "data.xml"


Node* Application::loadResources() {
	return loadXML(XML_FILENAME);
}

#if defined(__ANDROID__)
Application::Application(AAssetManager* android_asset_manager) {
    this->asset_manager = new AssetManager(android_asset_manager);
#elif defined(DESKTOP_APP)
Application::Application() {
    this->asset_manager = new AssetManager();
#endif
	assert(this->asset_manager);
    config_file_contents = 0;
    simulation = new Simulation();
	assert(simulation);
	scenegraph_root = loadResources();
	assert(scenegraph_root);
	init();
    if(config_file_contents) {
        delete [] config_file_contents;
        config_file_contents = 0;
    }
}

Application::~Application() {
	if (simulation) {
		delete simulation;
	}
	if (scenegraph_root) {
		scenegraph::destroy(scenegraph_root);
		scenegraph_root = 0;
	}
	for (std::map<std::string, Image*>::iterator it = images.begin();
			it != images.end(); ++it) {
		if (it->second) {
			delete it->second;
			it->second = 0;
		}
	}
	images.clear();
	if(asset_manager) {
		delete asset_manager;
	}
}

Node* Application::parseXML(rapidxml::xml_document<>& doc) {
	Node* scene_node = new Node();
	assert(scene_node);
	scene_node->name = std::string(doc.name()) + std::string(" node");
	parseXMLNode(doc.first_node(), scene_node);
	simulation->parseXMLNode(doc.first_node(), scene_node);
	return scene_node;
}

void Application::parseXMLNode(rapidxml::xml_node<>* my_xml_node,
		scenegraph::Node* scene_node) {
	if (!my_xml_node)
		return;
	char* c_name = my_xml_node->name();
	if (0 == strlen(c_name))
		return;
	std::string name(c_name);
	if (0 == std::string("WavefrontFile").compare(name)) {
		for (rapidxml::xml_attribute<> *attr = my_xml_node->first_attribute();
				attr; attr = attr->next_attribute()) {
			if (0 == std::string("filename").compare(attr->name())) {
				WavefrontSceneGraphFactory factory;
				bool status = factory.addWavefront(attr->value(), glm::mat4(1.f), asset_manager);
				assert(status);
				for (std::set<std::string>::iterator it = factory.textures.begin(); it != factory.textures.end(); ++it) {
					std::string s = *it;
					Image* t = new Image(s.c_str(), asset_manager);
					assert(t);
					images[s] = t;
				}
				Node* wf = factory.build();
				assert(wf);
				scene_node->children.push_back(wf);
			}
		}
	} else if (0 == std::string("InstanceNode").compare(name)) {
		LOGI("Instance Node %s", my_xml_node->value());
	}

	// Recursively parse children and siblings
	parseXMLNode(my_xml_node->first_node(), scene_node);
	parseXMLNode(my_xml_node->next_sibling(), scene_node);
}

Node* Application::loadXML(const char* xml_filename) {
	rapidxml::xml_document<> doc;
    config_file_contents = asset_manager->loadTextChars(xml_filename);
	doc.parse<0>(config_file_contents);
	return parseXML(doc);
}

void Application::init() {
	// Init camera
	camera = new Camera();
	assert(camera);
	camera->position.y = 4.0;
	camera->moveBackward(28.0);
	camera->update();
}

void Application::resize(int width, int height) {
	assert(camera);
	camera->projectionMatrix = glm::perspective(45.0f,
			(float) ((double) width / (double) height), 0.1f, 10000.0f);
	camera->update();
}

void Application::step() {
	if (!simulation)
		return;

	simulation->step();

	// Update scenegraph with physics simulation data
	for (int j = simulation->dynamics_world->getNumCollisionObjects() - 1;
			j >= 0; j--) {
		btCollisionObject* obj =
				simulation->dynamics_world->getCollisionObjectArray()[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState()) {
			body->getMotionState()->getWorldTransform(trans);
		} else {
			trans = obj->getWorldTransform();
		}

		std::map<int, PhysicsNode>::iterator itr =
				simulation->physics_nodes.find(j);
		if (itr != simulation->physics_nodes.end() && itr->second.mass > 0.f) {
			btVector3 origin_bt = trans.getOrigin();
			glm::vec3 pos((float) origin_bt.x(), (float) origin_bt.y(),
					(float) origin_bt.z());
			glm::mat4 pos_mat = glm::translate(glm::mat4(1.0f), pos);

			btQuaternion rotation_bt = trans.getRotation();
			glm::quat quat(rotation_bt.w(), rotation_bt.x(), rotation_bt.y(),
					rotation_bt.z());
			glm::mat4 rotation = glm::toMat4(quat);
			itr->second.transform_node->matrix = pos_mat * rotation;
		}
	}
}
