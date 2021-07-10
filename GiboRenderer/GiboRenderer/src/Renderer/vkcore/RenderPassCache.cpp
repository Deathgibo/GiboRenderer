#include "../../pch.h"
#include "RenderPassCache.h"

namespace Gibo {

	void RenderPassCache::PrintDebug() const
	{
		Logger::Log("-----RenderPassCache Info-----\n");
		Logger::Log("number of renderpasses stored: ", mRenderPassArray.size(), "capacity in vector: ", mRenderPassArray.capacity(), "\n");
	}

	void RenderPassCache::Cleanup()
	{
	/*	//free every renderpass
		for (auto renderpass : mRenderPassArray)
		{
			vkDestroyRenderPass(deviceref, renderpass, nullptr);
		}
		mRenderPassArray.clear();

	*/
	}

	VkRenderPass RenderPassCache::GetRenderPass(RenderPassAttachment* attachments, uint32_t attachment_count, VkPipelineBindPoint bindpoint)
	{
		VkRenderPass rp;

		std::vector<VkAttachmentDescription> rp_attachments;
		std::vector<VkAttachmentReference> rp_colorattachmentrefs;
		std::vector<VkAttachmentReference> rp_depthattachmentrefs;
		std::vector<VkAttachmentReference> rp_resolveattachmentrefs;
		std::vector<VkAttachmentReference> rp_inputattachmentrefs;

		for (int i = 0; i < attachment_count; i++)
		{
			RenderPassAttachment* current_attachment = attachments + i;

			VkAttachmentDescription Attachment = {};
			Attachment.format = current_attachment->mformat;
			Attachment.samples = current_attachment->msample_count;
			Attachment.loadOp = current_attachment->mloadop;
			Attachment.storeOp = current_attachment->mstoreop;
			Attachment.stencilLoadOp = current_attachment->mstencilloadop;
			Attachment.stencilStoreOp = current_attachment->mstencilstoreop;
			Attachment.initialLayout = current_attachment->mcurrent_layout; //undefined if you don't know. this will curropt it, but if you clear doesn't matter.
			Attachment.finalLayout = current_attachment->mfinal_layout;

			VkAttachmentReference AttachmentRef = {};
			AttachmentRef.attachment = current_attachment->mattachmentnumber;
			AttachmentRef.layout = current_attachment->minitial_layout; //transition at beggining of subpass

			rp_attachments.push_back(Attachment);

			switch (current_attachment->mtype)
			{
			case RENDERPASSTYPE::COLOR:     rp_colorattachmentrefs.push_back(AttachmentRef);    break;
			case RENDERPASSTYPE::DEPTH:     rp_depthattachmentrefs.push_back(AttachmentRef);    break;
			case RENDERPASSTYPE::RESOLVE:   rp_resolveattachmentrefs.push_back(AttachmentRef);  break;
			case RENDERPASSTYPE::INPUT:     rp_inputattachmentrefs.push_back(AttachmentRef);    break;
			}
		}

		if (rp_depthattachmentrefs.size() > 1 || rp_resolveattachmentrefs.size() > 1)
		{
			Logger::LogWarning("Render pass can't have more than 1 depth attachment or resolve attachment\n");
		}

		VkSubpassDescription subpass = {};
		subpass.flags = 0;
		subpass.pipelineBindPoint = bindpoint;
		subpass.colorAttachmentCount = rp_colorattachmentrefs.size();
		if (rp_colorattachmentrefs.size() != 0)
		{
			subpass.pColorAttachments = rp_colorattachmentrefs.data();
		}
		subpass.pDepthStencilAttachment = 0;
		if (rp_depthattachmentrefs.size() == 1)
		{
			subpass.pDepthStencilAttachment = rp_depthattachmentrefs.data();
		}
		subpass.pResolveAttachments = 0;
		if (rp_resolveattachmentrefs.size() == 1)
		{
			subpass.pResolveAttachments = rp_resolveattachmentrefs.data();
		}
		subpass.inputAttachmentCount = rp_inputattachmentrefs.size();
		if (rp_inputattachmentrefs.size() != 0)
		{
			subpass.pInputAttachments = rp_inputattachmentrefs.data();
		}
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = nullptr;
		renderPassInfo.flags = 0;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(rp_attachments.size());
		renderPassInfo.pAttachments = rp_attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 0;
		renderPassInfo.pDependencies;

		VULKAN_CHECK(vkCreateRenderPass(deviceref, &renderPassInfo, nullptr, &rp), "creating renderpass");

		mRenderPassArray.push_back(rp);
		return rp;
	}

	VkRenderPass RenderPassCache::GetRenderPass(RenderPassAttachment* attachments, uint32_t attachment_count, RenderPassSubPass* subpasses, uint32_t subpass_count, RenderPassDependency* dependencies, uint32_t dependency_count)
	{
		VkRenderPass rp;

		std::vector<VkAttachmentDescription> rp_attachments;
		std::vector<VkAttachmentReference> rp_attachmentrefs;
		for (int i = 0; i < attachment_count; i++)
		{
			RenderPassAttachment* current_attachment = attachments + i;

			VkAttachmentDescription Attachment = {};
			Attachment.format = current_attachment->mformat;
			Attachment.samples = current_attachment->msample_count;
			Attachment.loadOp = current_attachment->mloadop;
			Attachment.storeOp = current_attachment->mstoreop;
			Attachment.stencilLoadOp = current_attachment->mstencilloadop;
			Attachment.stencilStoreOp = current_attachment->mstencilstoreop;
			Attachment.initialLayout = current_attachment->mcurrent_layout; //undefined if you don't know. this will curropt it, but if you clear doesn't matter.
			Attachment.finalLayout = current_attachment->mfinal_layout;

			VkAttachmentReference AttachmentRef = {};
			AttachmentRef.attachment = current_attachment->mattachmentnumber;
			AttachmentRef.layout = current_attachment->minitial_layout; //transition at beggining of subpass

			rp_attachments.push_back(Attachment);

			rp_attachmentrefs.push_back(AttachmentRef);
		}

		std::vector<VkSubpassDescription> current_subpasses(subpass_count);
		for (int i = 0; i < subpass_count; i++)
		{
			RenderPassSubPass* pass = subpasses + i;

			std::vector<VkAttachmentReference> sp_colorattachmentrefs;
			for (int x = 0; x < pass->color_attachment.size(); x++)
			{
				sp_colorattachmentrefs.push_back(rp_attachmentrefs[pass->color_attachment[i]]);
			}
			std::vector<VkAttachmentReference> sp_depthattachmentrefs;
			for (int x = 0; x < pass->depthstencil_attachment.size(); x++)
			{
				sp_depthattachmentrefs.push_back(rp_attachmentrefs[pass->depthstencil_attachment[i]]);
			}
			std::vector<VkAttachmentReference> sp_resolveattachmentrefs;
			for (int x = 0; x < pass->resolve_attachment.size(); x++)
			{
				sp_resolveattachmentrefs.push_back(rp_attachmentrefs[pass->resolve_attachment[i]]);
			}
			std::vector<VkAttachmentReference> sp_inputattachmentrefs;
			for (int x = 0; x < pass->input_attachment.size(); x++)
			{
				sp_inputattachmentrefs.push_back(rp_attachmentrefs[pass->input_attachment[i]]);
			}

			VkSubpassDescription subpass = {};
			subpass.flags = 0;
			subpass.pipelineBindPoint = pass->bindpoint;
			subpass.colorAttachmentCount = sp_colorattachmentrefs.size();
			if (sp_colorattachmentrefs.size() != 0)
			{
				subpass.pColorAttachments = sp_colorattachmentrefs.data();
			}
			subpass.pDepthStencilAttachment = 0;
			if (sp_depthattachmentrefs.size() == 1)
			{
				subpass.pDepthStencilAttachment = sp_depthattachmentrefs.data();
			}
			subpass.pResolveAttachments = 0;
			if (sp_resolveattachmentrefs.size() == 1)
			{
				subpass.pResolveAttachments = sp_resolveattachmentrefs.data();
			}
			subpass.inputAttachmentCount = sp_inputattachmentrefs.size();
			if (sp_inputattachmentrefs.size() != 0)
			{
				subpass.pInputAttachments = sp_inputattachmentrefs.data();
			}
			subpass.preserveAttachmentCount = 0;
			subpass.pPreserveAttachments;

			current_subpasses.push_back(subpass);
		}


		std::vector<VkSubpassDependency> current_dependencies;
		for (int i = 0; i < dependency_count; i++)
		{
			RenderPassDependency* curr_depend = dependencies + i;

			VkSubpassDependency dependency = {};
			dependency.srcSubpass = curr_depend->srcsubpass;
			dependency.dstSubpass = curr_depend->dstsubpass;
			dependency.srcStageMask = curr_depend->srcstage;
			dependency.dstStageMask = curr_depend->dststage;
			dependency.srcAccessMask = curr_depend->srcaccess;
			dependency.dstAccessMask = curr_depend->dstaccess;
			dependency.dependencyFlags = 0;

			current_dependencies.push_back(dependency);
		}
		
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = nullptr;
		renderPassInfo.flags = 0;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(rp_attachments.size());
		renderPassInfo.pAttachments = rp_attachments.data();
		renderPassInfo.subpassCount = current_subpasses.size();
		renderPassInfo.pSubpasses = current_subpasses.data();
		renderPassInfo.dependencyCount = current_dependencies.size();
		renderPassInfo.pDependencies = current_dependencies.data();

		VULKAN_CHECK(vkCreateRenderPass(deviceref, &renderPassInfo, nullptr, &rp), "creating renderpass with subpasses/dependencies");

		mRenderPassArray.push_back(rp);
		return rp;
	}
}