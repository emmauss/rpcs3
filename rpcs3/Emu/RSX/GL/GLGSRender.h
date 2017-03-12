#pragma once
#include "Emu/RSX/GSRender.h"
#include "GLHelpers.h"
#include "GLTexture.h"
#include "GLTextureCache.h"
#include "GLRenderTargets.h"
#include "restore_new.h"
#include "Utilities/optional.hpp"
#include "define_new_memleakdetect.h"
#include "GLProgramBuffer.h"
#include "GLTextOut.h"

#pragma comment(lib, "opengl32.lib")

struct work_item
{
	std::condition_variable cv;
	std::mutex guard_mutex;
	
	u32  address_to_flush = 0;
	gl::texture_cache::cached_rtt_section *section_to_flush = nullptr;

	volatile bool processed = false;
	volatile bool result = false;
	volatile bool received = false;
};

struct gcm_buffer_info
{
	u32 address = 0;
	u32 pitch = 0;
	
	bool is_depth_surface;

	rsx::surface_color_format color_format;
	rsx::surface_depth_format depth_format;

	u16 width;
	u16 height;

	gcm_buffer_info()
	{
		address = 0;
		pitch = 0;
	}

	gcm_buffer_info(const u32 address_, const u32 pitch_, bool is_depth_, const rsx::surface_color_format fmt_, const rsx::surface_depth_format dfmt_, const u16 w, const u16 h)
		:address(address_), pitch(pitch_), is_depth_surface(is_depth_), color_format(fmt_), depth_format(dfmt_), width(w), height(h)
	{}
};

class GLGSRender : public GSRender
{
private:
	GLFragmentProgram m_fragment_prog;
	GLVertexProgram m_vertex_prog;

	rsx::gl::texture m_gl_textures[rsx::limits::fragment_textures_count];
	rsx::gl::texture m_gl_vertex_textures[rsx::limits::vertex_textures_count];

	gl::glsl::program *m_program;

	gl_render_targets m_rtts;

	gl::texture_cache m_gl_texture_cache;

	gl::texture m_gl_attrib_buffers[rsx::limits::vertex_count];
	
	std::unique_ptr<gl::ring_buffer> m_attrib_ring_buffer;
	std::unique_ptr<gl::ring_buffer> m_fragment_constants_buffer;
	std::unique_ptr<gl::ring_buffer> m_transform_constants_buffer;
	std::unique_ptr<gl::ring_buffer> m_scale_offset_buffer;
	std::unique_ptr<gl::ring_buffer> m_index_ring_buffer;

	u32 m_draw_calls = 0;
	u32 m_begin_time = 0;
	u32 m_draw_time = 0;
	u32 m_vertex_upload_time = 0;
	u32 m_textures_upload_time = 0;

	//Compare to see if transform matrix have changed
	size_t m_transform_buffer_hash = 0;
	
	GLint m_min_texbuffer_alignment = 256;
	GLint m_uniform_buffer_offset_align = 256;

	bool manually_flush_ring_buffers = false;

	gl::text_writer m_text_printer;

	std::mutex queue_guard;
	std::list<work_item> work_queue;

	gcm_buffer_info surface_info[rsx::limits::color_buffers_count];
	gcm_buffer_info depth_surface_info;

	bool flush_draw_buffers = false;

public:
	gl::fbo draw_fbo;

private:
	GLProgramBuffer m_prog_buffer;

	//buffer
	gl::fbo m_flip_fbo;
	gl::texture m_flip_tex_color;

	//vaos are mandatory for core profile
	gl::vao m_vao;

public:
	GLGSRender();

private:
	static u32 enable(u32 enable, u32 cap);
	static u32 enable(u32 enable, u32 cap, u32 index);

	// Return element to draw and in case of indexed draw index type and offset in index buffer
	std::tuple<u32, std::optional<std::tuple<GLenum, u32> > > set_vertex_buffer();

	void clear_surface(u32 arg);

public:
	bool load_program();
	void init_buffers(bool skip_reading = false);
	void read_buffers();
	void write_buffers();
	void set_viewport();

	void synchronize_buffers();
	work_item& post_flush_request(u32 address, gl::texture_cache::cached_rtt_section *section);

protected:
	void begin() override;
	void end() override;

	void on_init_thread() override;
	void on_exit() override;
	bool do_method(u32 id, u32 arg) override;
	void flip(int buffer) override;
	u64 timestamp() const override;

	void do_local_task() override;

	bool on_access_violation(u32 address, bool is_writing) override;

	virtual std::array<std::vector<gsl::byte>, 4> copy_render_targets_to_memory() override;
	virtual std::array<std::vector<gsl::byte>, 2> copy_depth_stencil_buffer_to_memory() override;
};
