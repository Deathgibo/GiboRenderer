#pragma once
#include "vkcore/vkcoreDevice.h"
#include "vkcore/ShaderProgram.h"
#include "glfw_handler.h"
#include "../Utilities/Input.h"
#include "AssetManager.h"
#include "TextureCache.h"
#include "Atmosphere.h"
#include "LightManager.h"
#include "RenderObjectManager.h"

namespace Gibo {

	struct Cluster;
	class RenderManager
	{
		enum SAMPLE_COUNT : int { ONE, TWO, FOUR, EIGHT, SIXTEEN, THIRTYTWO };
		enum RESOLUTION { THREESIXTYP, FOUREIGHTYP, SEVENTWENTYP, TENEIGHTYP, FOURTEENFORTYP, FOURKP };
		VkSampleCountFlagBits ConvertSampleCount(SAMPLE_COUNT x)
		{
			switch (x)
			{
			case ONE: return VK_SAMPLE_COUNT_1_BIT; break;
			case TWO: return VK_SAMPLE_COUNT_2_BIT; break;
			case FOUR: return VK_SAMPLE_COUNT_4_BIT; break;
			case EIGHT: return VK_SAMPLE_COUNT_8_BIT; break;
			case SIXTEEN: return VK_SAMPLE_COUNT_32_BIT; break;
			case THIRTYTWO: return VK_SAMPLE_COUNT_64_BIT; break;
			}

			return VK_SAMPLE_COUNT_1_BIT;
		}

		void ConvertResolution(RESOLUTION res, int& width, int& height)
		{
			switch (res)
			{
			case THREESIXTYP:	 width = 640;  height = 360;  break;
			case FOUREIGHTYP:	 width = 720;  height = 480;  break;
			case SEVENTWENTYP:   width = 1280; height = 720;  break;
			case TENEIGHTYP:	 width = 1920; height = 1080; break; 
			case FOURTEENFORTYP: width = 2560; height = 1440; break;
			case FOURKP:		 width = 3840; height = 2160; break;
			}
		}

	public:
		struct descriptorgraveinfo
		{
			uint32_t id;
			int frame_count;

			descriptorgraveinfo(uint32_t id_, int frame_count_) : id(id_), frame_count(frame_count_) {};
		};

		struct reduce_struct {
			glm::mat4 proj;
			float n;
			float f;
			float a;
			float b;
		};
	public:
		RenderManager();
		~RenderManager();

		bool InitializeRenderer();
		void ShutDownRenderer();

		//no copying/moving should be allowed from this class
		// disallow copy and assignment
		RenderManager(RenderManager const&) = delete;
		RenderManager(RenderManager&&) = delete;
		RenderManager& operator=(RenderManager const&) = delete;
		RenderManager& operator=(RenderManager&&) = delete;
		

		void AddRenderObject(RenderObject* object, RenderObjectManager::BIN_TYPE type);
		void RemoveRenderObject(RenderObject* object, RenderObjectManager::BIN_TYPE type);

		void Update();
		void Render();
		void Recreateswapchain();
		void SetMultisampling(SAMPLE_COUNT count); 
		void SetResolution(RESOLUTION res);
		void SetFullScreen(bool val) { WindowManager.SetFullScreen(1920, 1080); };
		void SetCameraInfo(glm::mat4 cammatrix, glm::vec3 campos) {
			cam_matrix = cammatrix;
			atmosphere->UpdateCamPosition(glm::vec4(campos, 1));
		}

		void UpdateSunPosition(glm::vec4 pos) {
			atmosphere->UpdateSunPosition(pos);
		}

		void UpdateFrustrumClusters();
		void SetResolutionFitted(bool val) { Resolution_Fitted = val; }
		bool IsWindowOpen() const;
		void SetWindowTitle(std::string title) const;
		Input& GetInputManager() { return InputManager; }
		glfw_handler GetWindowManager() { return WindowManager; }
		vkcoreDevice* GetDevice() { return &Device; }
		int GetFramesInFlight() { return FRAMES_IN_FLIGHT; }
		MeshCache* GetMeshCache() { return meshCache; }
		TextureCache* GetTextureCache() { return textureCache; }
		LightManager* GetLightManager() { return lightmanager; }
		RenderObjectManager* GetObjectManager() { return objectmanager; }
	private:
		void CreatePBR();
		void createPBRfinal();
		void CleanUpPBR();
		void PBRdeleteimagedata();
		void PBRcreateimagedata();
		void RecordPBRCmd(int current_frame);

		void CreateDepth();
		void CleanUpDepth();
		void Depthdeleteimagedata();
		void Depthcreateimagedata();
		void createDepthfinal();
		void RecordDepthCmd(int current_frame);
		
		void CreateReduce();
		void CleanUpReduce();
		void Reducecreateimagedata();
		void Reducedeleteimagedata();
		void createReducefinal();
		void UpdateReduceProjMatrix();
		void RecordReduceCmd(int current_frame);

		void CreateCluster();
		void CleanUpCluster();
		void Clustercreateimagedata();
		void Clusterdeleteimagedata();
		void createClusterfinal();
		void RecordClusterCmd(int current_frame);

		void CreateCompute();
		void CleanUpCompute();
		void Computecreateimagedata();
		void Computedeleteimagedata();
		void RecordComputeCmd(int current_frame, int current_imageindex);

		void CreateGui();
		void CleanUpGui();
		void Guicreateimagedata();
		void Guideleteimagedata();
		void RecordGuiCmd(int current_frame);

		void CleanUpGeneral();
		void CreateAtmosphere();

		void CreateShadow();
		void createShadowFinal();
		void CleanUpShadow();
		void Shadowdeleteimagedata();
		void Shadowcreateimagedata();
		void RecordShadowCmd(int current_frame);
		void RecordShadowHelper(int32_t offsetx, int32_t offsety, int index, int current_frame, glm::mat4 PV);

		void CreateQuad();
		void CleanUpQuad();
		void RecordQuadCmd(int current_frame, int current_imageindex);
		void Quadcreateimagedata();
		void Quaddeleteimagedata();

		void CreateBV();
		void CleanUpBV();
		void BVcreateimagedata();
		void BVdeleteimagedata();
		void UpdateBV();

		void startImGui();
		void updateImGui(int current_frame, int imageindex);
		void closeImGui();

		void UpdateDescriptorGraveYard();
		void UpdateShadowLights(int current_frame);
		void SortBlendedObjects();
		void SetProjectionMatrix();
		void SetCameraMatrix();
	private:
		Input InputManager; //1030 bytes
		vkcoreDevice Device; //8 bytes
		glfw_handler WindowManager; //16 bytes
		MeshCache* meshCache;
		TextureCache* textureCache;
		Atmosphere* atmosphere;
		LightManager* lightmanager;
		RenderObjectManager* objectmanager;

		bool enable_imgui = true;
		VkExtent2D window_extent{ 800, 640 };
		VkSampleCountFlagBits multisampling_count = VK_SAMPLE_COUNT_1_BIT;
		VkExtent2D Resolution = { 800, 640 };
		bool Resolution_Fitted = true;
		float FOV = 45.0f;
		float near_plane = 0.5f;
		float far_plane = 1000.0f;

		//imgui variables
		int imguimulti_count = 0;
		bool multi_changed = false;
		bool full_screenchanged = false;
		bool imguifullscreen = false;
		float imguifov = 45;
		int imguiresolution = 0;
		bool resolution_changed = false;

		//pbr
		VkFormat main_color_format;
		VkRenderPass renderpass_pbr;

		std::vector<vkcoreImage> color_attachment;
		std::vector<VkImageView> color_attachmentview;
		std::vector<vkcoreImage> depth_attachment;
		std::vector<VkImageView> depth_view;
		std::vector<vkcoreImage> resolve_attachment;
		std::vector<VkImageView> resolve_attachmentview;

		std::vector<VkFramebuffer> framebuffer_pbr;
		ShaderProgram program_pbr;
		vkcorePipeline pipeline_pbr;
		std::vector<VkCommandBuffer> cmdbuffer_pbr;

		std::vector<vkcoreBuffer> cascade_buffer;
		std::vector<vkcoreBuffer> point_pbrbuffer;
		std::vector<vkcoreBuffer> point_infobuffer;
		vkcoreBuffer nearfar_buffer;

		//post-processing
		std::vector<vkcoreImage> ppcolor_attachment;
		std::vector<VkImageView> ppcolor_attachmentview;
		ShaderProgram program_compute;
		vkcorePipeline pipeline_compute;
		std::vector<VkCommandBuffer> cmdbuffer_compute;

		//imgui
		VkDescriptorPool imguipool;
		VkRenderPass renderpass_imgui;
		std::vector<VkCommandBuffer> cmdbuffer_imgui;
		std::vector<VkFramebuffer> framebuffer_imgui;

		//guioverlay
		VkRenderPass renderpass_gui;
		std::vector<VkFramebuffer> framebuffer_gui;
		ShaderProgram program_gui;
		vkcorePipeline pipeline_gui;
		std::vector<VkCommandBuffer> cmdbuffer_gui;
		MeshCache::Mesh shadowmesh_gui;

		//quad
		VkRenderPass renderpass_quad;
		std::vector<VkFramebuffer> framebuffers_quad;
		ShaderProgram program_quad;
		vkcorePipeline pipeline_quad;
		std::vector<VkCommandBuffer> cmdbuffer_quad;
		VkSampler sampler_quad;
		MeshCache::Mesh mesh_quad;

		//depth presspass
		VkRenderPass renderpass_depth;
		std::vector<vkcoreImage> depthprepass_attachment;
		std::vector<VkImageView> depthprepass_view;
		std::vector<VkFramebuffer> framebuffers_depth;
		ShaderProgram program_depth;
		vkcorePipeline pipeline_depth;
		std::vector<VkCommandBuffer> cmdbuffer_depth;

		//reduce
		std::vector<std::vector<vkcoreImage>> reduce_images;
		std::vector<std::vector<VkImageView>> reduce_views;
		ShaderProgram program_reduce;
		vkcorePipeline pipeline_reduce;
		ShaderProgram program_reduce2;
		vkcorePipeline pipeline_reduce2;
		std::vector<VkCommandBuffer> cmdbuffer_reduce;
		reduce_struct reduce_data;
		vkcoreBuffer reduce_buffer; //might bug out a frame if you change projection/near/far plane but who cares
		vkcoreBuffer reduce_stagingbuffer;
		std::vector<std::vector<VkDescriptorSet>> reduce_descriptors;
		std::vector<VkExtent2D> reduce_extents;
		vkcoreImage dummyimageMS;
		VkImageView dummyviewMS;
		vkcoreBuffer minmax_buffer;

		//shadows
		VkRenderPass renderpass_shadow;
		VkRenderPass renderpass_shadowpoint;
		std::vector<vkcoreImage> shadowcascade_atlasimage;
		std::vector<VkImageView> shadowcascade_atlasview;
		std::vector<VkFramebuffer> framebuffer_shadowcascadesatlas;
		std::vector<vkcoreImage> shadowpoint_atlasimage;
		std::vector<VkImageView> shadowpoint_atlasview;
		std::vector<VkFramebuffer> framebuffer_shadowpoints;
		std::vector<std::vector<vkcoreBuffer>> shadowcascade_pv_buffers;
		std::vector<std::vector<vkcoreBuffer>> shadowcascade_nearplane_buffers;
		std::vector<vkcoreBuffer>  cascade_depthbuffers;
		std::vector<std::vector<vkcoreBuffer>> shadowpoint_pv_buffers;
		VkSampler shadowmapsampler;
		VkSampler shadowcubemapsampler;
		ShaderProgram program_shadow;
		ShaderProgram program_shadowpoint;
		vkcorePipeline pipeline_shadow;
		vkcorePipeline pipeline_shadowpoint;
		std::vector<VkCommandBuffer> cmdbuffer_shadow;
		std::vector<float> cascade_nears; //need this to pancake everythig to each cascades orthogonalplane
		std::vector<glm::vec4> cascade_depths;
		std::vector<glm::mat4> cascade_p;
		std::vector<glm::mat4> cascade_v;
		std::vector<glm::mat4> spot_p;
		std::vector<glm::mat4> spot_v;
		std::vector<glm::mat4> point_p;
		std::vector<glm::mat4> point_v;

		//cascade options
		glm::vec4 bias_info = glm::vec4(0.002, 0.002, 0.002, 0); //constant, normal, slope, not used
		int pcf_method = 0;
		const int CASCADE_COUNT = 4;
		const int MAX_CASCADES = 6; //needs to be updated with shaders
		bool SDSM_ENABLE = true;
		bool STABLE_ENABLE = false;
		uint32_t shadow_width = 1024 * 2;
		uint32_t shadow_height = 1024 * 2;
		uint32_t atlas_width = shadow_width * 2;
		uint32_t atlas_height = shadow_height *std::ceil(CASCADE_COUNT / 2.0f);
		//this is how far back the lights camera is from center of frustrum. Should be large enough to fit whole frustrum.
		//Really it can be as far back as it needs only problem is maybe precision errors if it gets so big near and far plane will be 20000, etc
		float cascade_sun_distance = 20000.0f;  //5000
		VkFormat cascadedshadow_map_format = VK_FORMAT_D16_UNORM; //VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM  TODO- why doesn't D32_float work?
		
		//point/spot options
		int lightshadow_counter = 0;
		std::vector<int> pointsize_current;
		std::vector<int> spotsize_current;
		int point_current = 0;
		int spot_current = 0;
		VkFormat pointspot_format = VK_FORMAT_D16_UNORM;
		uint32_t shadowpoint_width = 512 * 1;
		uint32_t shadowpoint_height = 512 * 1;
		uint32_t atlaspoint_width = 1024 * 2;
		uint32_t atlaspoint_height = 1024 * 2;
		uint32_t MAX_POINT_IMAGES = (atlaspoint_width / shadowpoint_width) * (atlaspoint_height / shadowpoint_height); //update to shader to match this number
		glm::vec4 point_info;

		std::array<float, 30> time_depth;
		std::array<float, 30> time_reduce;
		std::array<float, 30> time_cluster;
		std::array<float, 30> time_shadow;
		std::array<float, 30> time_mainpass;
		std::array<float, 30> time_quad;
		std::array<float, 30> time_pp;
		std::array<float, 30> time_cpu;
		int time_counter = 0;

		glm::mat4 proj_matrix;
		glm::mat4 cam_matrix = glm::mat4(1.0f);
		glm::vec3 cam_position;
		std::vector<vkcoreBuffer> pv_uniform;

		//Bounding Volumes
		bool Display_BV = false;
		bool OldDisplay_BV = false;
		ShaderProgram program_bv;
		vkcorePipeline pipeline_bv;
		std::vector<vkcoreBuffer> bv_vbos;
		std::vector<int> bv_vbosizes;
		vkcoreBuffer frustrum_vbo;
		vkcoreBuffer cluster_vbo;
		int cluster_vbo_count;
		glm::mat4 debug_cam_matrix;
		glm::mat4 debug_proj_matrix;
		glm::mat4 debug_ortho_matrix;
		float debug_theta = 270.0f;
		float debug_phi = 90.0f;
		float debug_far = 10.0f;
		float debug_near = 0.1f;

		//Clusters
		ShaderProgram program_clustervisible;
		ShaderProgram program_clustercull;
		vkcorePipeline pipeline_clustervisible;
		vkcorePipeline pipeline_clustercull;
		std::vector<VkCommandBuffer> cmdbuffer_cluster;
		std::vector<vkcoreBuffer> visible_clusters_storage;
		uint32_t* visible_clusters_data;
		vkcoreBuffer all_clusters_storage;
		std::vector<vkcoreBuffer> clusters_index_storage;
		std::vector<vkcoreBuffer> clusters_grid_storage;
		std::vector<Cluster> frustrum_clusters;
		std::vector<glm::mat4> clustercull_invmatrixes;
		int CLUSTER_X = 8;
		int CLUSTER_Y = 8;
		int CLUSTER_Z = 15;
		int CLUSTER_SIZE = CLUSTER_X * CLUSTER_Y * CLUSTER_Z; //must be lower than 1024 max local work group size

		//submission synchronization
		std::vector<VkFence> inFlightFences; 
		std::vector<VkFence> swapimageFences;
		std::vector<VkSemaphore> semaphore_imagefetch;
		std::vector<VkSemaphore> semaphore_depth;
		std::vector<VkSemaphore> semaphore_reduce;
		std::vector<VkSemaphore> semaphore_cluster;
		std::vector<VkSemaphore> semaphore_shadow;
		std::vector<VkSemaphore> semaphore_colorpass;
		std::vector<VkSemaphore> semaphore_gui;
		std::vector<VkSemaphore> semaphore_quad;
		std::vector<VkSemaphore> semaphore_computepass;
		std::vector<VkSemaphore> semaphore_imgui;

		int FRAMES_IN_FLIGHT = 3; //Make sure to test with different number
		int current_frame_in_flight = 0;

		std::vector<descriptorgraveinfo> descriptorgraveyard;

	};
}

