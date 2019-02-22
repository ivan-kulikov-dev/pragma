#include "stdafx_cengine.h"

// Link Libraries
#pragma comment(lib,"prosper.lib")
#pragma comment(lib,"Anvil.lib")
#pragma comment(lib,"vulkan-1.lib")
#pragma comment(lib,"datasystem.lib")
#pragma comment(lib,"cmaterialsystem.lib")
#pragma comment(lib,"materialsystem.lib")
#pragma comment(lib,"wgui.lib")
#pragma comment(lib,"vfilesystem.lib")
#pragma comment(lib,"engine.lib")
#pragma comment(lib,"shared.lib")
#pragma comment(lib,"client.lib")
#pragma comment(lib,"glfw3dll.lib")
#pragma comment(lib,"glfw.lib")
#ifdef USE_LUAJIT
	#pragma comment(lib,"lua51.lib")
#else
	#pragma comment(lib,"lua530.lib")
#endif
#pragma comment(lib,"luasystem.lib")
#pragma comment(lib,"util.lib")
#pragma comment(lib,"mathutil.lib")
#pragma comment(lib,"util_zip.lib")
#pragma comment(lib,"alsoundsystem.lib")
#pragma comment(lib,"luabind.lib")
#pragma comment(lib,"Shlwapi.lib")
#pragma comment(lib,"bzip2.lib")
//

#include "pragma/c_engine.h"
#include <wgui/wgui.h>
#include "cmaterialmanager.h"
#include "pragma/console/c_cvar.h"
#include <pragma/serverstate/serverstate.h>
#include <texturemanager/texturemanager.h>
#include <pragma/performancetimer.h>
#include "pragma/gui/wiimageslideshow.h"
#include "pragma/gui/mainmenu/wimainmenu.h"
#include <pragma/console/convars.h>
#include "pragma/console/engine_cvar.h"
#include <pragma/util/profiling_stages.h>
#include "pragma/rendering/uniformbinding.h"
#include "pragma/rendering/c_sci_gpu_timer_manager.hpp"
#include <pragma/entities/environment/lights/c_env_light.h>
#include <pragma/rendering/lighting/c_light.h>
#include <cmaterialmanager.h>
#include <pragma/model/c_modelmesh.h>
#include <cctype>
#include <sharedutils/util_debug.h>
#include <util_zip.h>
#include <pragma/input/inputhelper.h>
#include <fsys/directory_watcher.h>
#include <pragma/game/game_resources.hpp>
#include <impl_texture_formats.h>
#include <sharedutils/util_file.h>
#include <pragma/engine_info.hpp>
#include <prosper_util.hpp>
#include <image/prosper_render_target.hpp>
#include <debug/prosper_debug.hpp>
#include <prosper_command_buffer.hpp>
#include <pragma/entities/components/c_render_component.hpp>
#include <pragma/entities/environment/effects/c_env_particle_system.h>
#include <pragma/rendering/shaders/image/c_shader_clear_color.hpp>
#include <pragma/rendering/shaders/image/c_shader_gradient.hpp>

extern "C"
{
	void DLLCENGINE RunCEngine(int argc,char *argv[])
	{
		auto en = InitializeEngine<CEngine>(argc,argv);
		if(en == nullptr)
			return;
		en->Release(); // Has to be called before object is actually destroyed, to make sure weak_ptr references are still valid
		en = nullptr;
	}
}

DLLCENGINE CEngine *c_engine = NULL;
extern DLLCLIENT ClientState *client;
extern DLLCLIENT CGame *c_game;
//__declspec(dllimport) std::vector<void*> _vkImgPtrs;
decltype(CEngine::AXIS_PRESS_THRESHOLD) CEngine::AXIS_PRESS_THRESHOLD = 0.5f;

// If set to true, each joystick axes will be split into a positive and a negative axis, which
// can be bound individually
static const auto SEPARATE_JOYSTICK_AXES = true;

CEngine::CEngine(int argc,char* argv[])
	: Engine(argc,argv),pragma::RenderContext(),
	m_nearZ(1.f),//10.0f), //0.1f
	m_farZ(32768.0f),
	m_fps(0),m_tFPSTime(0.f),
	m_bUniformBlocksInitialized(false),
	m_tLastFrame(std::chrono::high_resolution_clock::now()),m_tDeltaFrameTime(0)
{
	c_engine = this;

	RegisterCallback<void,std::reference_wrapper<const GLFW::Joystick>,bool>("OnJoystickStateChanged");
	RegisterCallback<void,std::reference_wrapper<std::shared_ptr<prosper::PrimaryCommandBuffer>>>("DrawFrame");
	RegisterCallback<void,std::reference_wrapper<std::shared_ptr<prosper::PrimaryCommandBuffer>>>("PreDrawGUI");
	RegisterCallback<void,std::reference_wrapper<std::shared_ptr<prosper::PrimaryCommandBuffer>>>("PostDrawGUI");
	RegisterCallback<void>("Draw");
}

void CEngine::Release()
{
	Close();
	Engine::Release();
	pragma::RenderContext::Release();
}

void CEngine::StartGPUTimer(GPUTimerEvent ev) const {m_gpuTimerManager->StartTimer(ev);}
void CEngine::StopGPUTimer(GPUTimerEvent ev) const {m_gpuTimerManager->StopTimer(ev);}
bool CEngine::GetGPUTimerResult(GPUTimerEvent ev,float &r) const {return m_gpuTimerManager->GetResult(ev,r);}
float CEngine::GetGPUTimerResult(GPUTimerEvent ev) const {return m_gpuTimerManager->GetResult(ev);}
CSciGPUTimerManager &CEngine::GetGPUTimerManager() const {return *m_gpuTimerManager;}

void CEngine::DumpDebugInformation(ZIPFile &zip) const
{
	Engine::DumpDebugInformation(zip);
	std::stringstream ss;
	auto &dev = c_engine->GetDevice();
	auto &gpuProperties = dev.get_physical_device_properties();
	ss<<"Vulkan API Version: "<<gpuProperties.core_vk1_0_properties_ptr->api_version<<"\n";
	ss<<"Device Name: "<<gpuProperties.core_vk1_0_properties_ptr->device_name<<"\n";
	ss<<"Device Type: "<<prosper::util::to_string(static_cast<vk::PhysicalDeviceType>(gpuProperties.core_vk1_0_properties_ptr->device_type))<<"\n";
	ss<<"Driver Version: "<<gpuProperties.core_vk1_0_properties_ptr->driver_version<<"\n";
	ss<<"Vendor ID: "<<gpuProperties.core_vk1_0_properties_ptr->vendor_id;
	zip.AddFile("gpu.txt",ss.str());
	
	ss.str(std::string());
	ss.clear();
	prosper::debug::dump_layers(*c_engine,ss);
	zip.AddFile("vk_layers.txt",ss.str());

	ss.str(std::string());
	ss.clear();
	prosper::debug::dump_extensions(*c_engine,ss);
	zip.AddFile("vk_extensions.txt",ss.str());

	ss.str(std::string());
	ss.clear();
	prosper::debug::dump_limits(*c_engine,ss);
	zip.AddFile("vk_limits.txt",ss.str());

	ss.str(std::string());
	ss.clear();
	prosper::debug::dump_features(*c_engine,ss);
	zip.AddFile("vk_features.txt",ss.str());

	ss.str(std::string());
	ss.clear();
	prosper::debug::dump_image_format_properties(*c_engine,ss);
	zip.AddFile("vk_image_format_properties.txt",ss.str());

	ss.str(std::string());
	ss.clear();
	prosper::debug::dump_format_properties(*c_engine,ss);
	zip.AddFile("vk_format_properties.txt",ss.str());
}

void CEngine::InitializeStagingTarget()
{
	c_engine->WaitIdle();
	auto &dev = GetDevice();
	prosper::util::ImageCreateInfo createInfo {};
	createInfo.usage = Anvil::ImageUsageFlagBits::TRANSFER_DST_BIT | Anvil::ImageUsageFlagBits::TRANSFER_SRC_BIT | Anvil::ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;
	createInfo.format = Anvil::Format::R8G8B8A8_UNORM;
	createInfo.width = GetWindowWidth();
	createInfo.height = GetWindowHeight();
	createInfo.postCreateLayout = Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
	auto stagingImg = prosper::util::create_image(dev,createInfo);
	prosper::util::ImageViewCreateInfo imgViewCreateInfo {};
	auto stagingTex = prosper::util::create_texture(dev,{},stagingImg,&imgViewCreateInfo);

	auto rp = prosper::util::create_render_pass(dev,
		prosper::util::RenderPassCreateInfo{{{Anvil::Format::R8G8B8A8_UNORM,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,Anvil::AttachmentLoadOp::DONT_CARE,
			Anvil::AttachmentStoreOp::STORE,Anvil::SampleCountFlagBits::_1_BIT,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL
		}}}
		//prosper::util::RenderPassCreateInfo{vk::Format::eD32Sfloat,vk::ImageLayout::eDepthStencilAttachmentOptimal,vk::AttachmentLoadOp::eClear}
	);
	m_bFirstFrame = true;
	m_stagingRenderTarget = prosper::util::create_render_target(dev,{stagingTex},rp);//,finalDepthTex},rp);
	m_stagingRenderTarget->SetDebugName("engine_staging_rt");
	// Vulkan TODO: Resize when window resolution was changed
}

UInt32 CEngine::GetFPS() const {return m_fps;}
UInt32 CEngine::GetFrameTime() const {return CUInt32(m_tFPSTime *1000.f);}
Double CEngine::GetDeltaFrameTime() const {return std::chrono::duration_cast<std::chrono::nanoseconds>(m_tDeltaFrameTime).count() /1'000'000'000.0;}

static auto cvFrameLimit = GetClientConVar("cl_max_fps");
uint32_t CEngine::GetFPSLimit() const {return cvFrameLimit->GetInt();}

unsigned int CEngine::GetStereoSourceCount() {return 0;}
unsigned int CEngine::GetMonoSourceCount() {return 0;}
unsigned int CEngine::GetStereoSource(unsigned int idx) {return 0;}
float CEngine::GetNearZ() {return m_nearZ;}
float CEngine::GetFarZ() {return m_farZ;}

bool CEngine::IsClientConnected()
{
	auto *cl = static_cast<ClientState*>(GetClientState());
	if(cl == nullptr)
		return false;
	return cl->IsConnected();
}

void CEngine::EndGame()
{
	Disconnect();
	auto *cl = GetClientState();
	if(cl != nullptr)
		cl->EndGame();
	Engine::EndGame();
}

void CEngine::Input(int key,GLFW::KeyState inputState,GLFW::KeyState pressState,GLFW::Modifier mods,float magnitude)
{
	if(key == static_cast<int>(GLFW::Key::Apostrophe))
	{
		if(pressState == GLFW::KeyState::Press)
		{
			if(IsConsoleOpen())
				;//CloseConsole();
			else
				OpenConsole();
		}
		return;
	}
	if(inputState == GLFW::KeyState::Press || inputState == GLFW::KeyState::Release || inputState == GLFW::KeyState::Held)
	{
		if((mods &GLFW::Modifier::AxisNegative) != GLFW::Modifier::None)
		{
			// We need to check if there are any keybindings with a command with the JoystickAxisSingle flag set,
			// in which case that keybinding has priority
			auto keyPositive = key -1;
			auto it = m_keyMappings.find(CInt16(keyPositive));
			if(it != m_keyMappings.end() && it->second.Execute(inputState,pressState,mods,magnitude) == true)
				return;
			mods &= ~GLFW::Modifier::AxisNegative;
		}
		auto it = m_keyMappings.find(CInt16(key));
		if(it != m_keyMappings.end())
			it->second.Execute(inputState,pressState,mods,magnitude);
	}
}
void CEngine::Input(int key,GLFW::KeyState state,GLFW::Modifier mods,float magnitude) {Input(key,state,state,mods,magnitude);}
void CEngine::MouseInput(GLFW::Window &window,GLFW::MouseButton button,GLFW::KeyState state,GLFW::Modifier mods)
{
	if(client != nullptr && client->RawMouseInput(button,state,mods) == false)
		return;
	if(WGUI::GetInstance().HandleMouseInput(window,button,state,mods))
		return;
	button += static_cast<GLFW::MouseButton>(GLFW::Key::Last);
	if(client != nullptr && client->MouseInput(button,state,mods) == false)
		return;
	Input(static_cast<int>(button),state);
}
void CEngine::GetMappedKeys(const std::string &cvarName,std::vector<GLFW::Key> &keys,uint32_t maxKeys)
{
	if(maxKeys != std::numeric_limits<uint32_t>::max())
		keys.reserve(maxKeys);
	if(maxKeys == 0)
		return;
	std::string cmd;
	std::vector<std::string> argv {};
	ustring::get_sequence_commands(cvarName,[&cmd,&argv](std::string cmdOther,std::vector<std::string> argvOther) {
		cmd = cmdOther;
		argv = argvOther;
	});
	auto &keyMappings = GetKeyMappings();
	for(auto &pair : keyMappings)
	{
		auto &keyBind = pair.second;
		auto bFoundCvar = false;
		ustring::get_sequence_commands(keyBind.GetBind(),[&cmd,&argv,&bFoundCvar](std::string cmdOther,std::vector<std::string> &argvOther) {
			if(cmdOther == "toggle" && argvOther.empty() == false)
				cmdOther += " " +argvOther.front();
			if(cmdOther == cmd && argv.size() == argvOther.size())
			{
				auto bDiscrepancy = false;
				for(auto i=decltype(argv.size()){0};i<argv.size();++i)
				{
					if(argv.at(i) == argvOther.at(i))
						continue;
					bDiscrepancy = true;
					break;
				}
				if(bDiscrepancy == false)
					bFoundCvar = true;
			}
		});
		if(bFoundCvar == true)
		{
			if(keys.size() == keys.capacity())
				keys.reserve(keys.size() +10);
			keys.push_back(static_cast<GLFW::Key>(pair.first));
			if(keys.size() == maxKeys)
				break;
		}
	}
}
void CEngine::JoystickButtonInput(GLFW::Window &window,const GLFW::Joystick &joystick,uint32_t key,GLFW::KeyState state)
{
	KeyboardInput(window,static_cast<GLFW::Key>(key),-1,state,{});
}
void CEngine::JoystickAxisInput(GLFW::Window &window,const GLFW::Joystick &joystick,uint32_t axis,GLFW::Modifier mods,float newVal,float deltaVal)
{
	auto oldVal = newVal -deltaVal;
	auto key = static_cast<GLFW::Key>(axis);
	auto state = (IsValidAxisInput(newVal) == true) ? GLFW::KeyState::Press : GLFW::KeyState::Release;
	auto it = m_joystickKeyStates.find(key);
	auto oldState = (it == m_joystickKeyStates.end()) ? GLFW::KeyState::Release : it->second;
	if(state == GLFW::KeyState::Release && oldState == GLFW::KeyState::Release)
		return;
	if(state == GLFW::KeyState::Press && oldState == GLFW::KeyState::Press)
		state = GLFW::KeyState::Held;

	m_joystickKeyStates[key] = state;
	mods |= GLFW::Modifier::AxisInput;
	if(umath::abs(newVal) > AXIS_PRESS_THRESHOLD)
	{
		if(umath::abs(oldVal) <= AXIS_PRESS_THRESHOLD)
			mods |= GLFW::Modifier::AxisPress; // Axis represents actual button press
	}
	else if(umath::abs(oldVal) > AXIS_PRESS_THRESHOLD)
		mods |= GLFW::Modifier::AxisRelease; // Axis represents actual button release
	KeyboardInput(window,key,-1,state,mods,newVal);
}
static auto cvAxisInputThreshold = GetClientConVar("cl_controller_axis_input_threshold");
bool CEngine::IsValidAxisInput(float axisInput) const
{
	return (umath::abs(axisInput) > cvAxisInputThreshold->GetFloat()) ? true : false;
}
bool CEngine::GetInputButtonState(float axisInput,GLFW::Modifier mods,GLFW::KeyState &inOutState) const
{
	if(IsValidAxisInput(axisInput) == false)
	{
		if((mods &GLFW::Modifier::AxisInput) != GLFW::Modifier::None)
		{
			inOutState = GLFW::KeyState::Release;
			return true;
		}
		inOutState = GLFW::KeyState::Invalid;
		return false;
	}
	if((mods &GLFW::Modifier::AxisInput) == GLFW::Modifier::None)
		return true; // No need to change state

	if((mods &GLFW::Modifier::AxisPress) != GLFW::Modifier::None)
		inOutState = GLFW::KeyState::Press;
	else if((mods &GLFW::Modifier::AxisRelease) != GLFW::Modifier::None)
		inOutState = GLFW::KeyState::Release;
	else
	{
		inOutState = GLFW::KeyState::Invalid;
		return false; // Not an actual key press
	}
	return true;
}
void CEngine::KeyboardInput(GLFW::Window &window,GLFW::Key key,int scanCode,GLFW::KeyState state,GLFW::Modifier mods,float magnitude)
{
	if(client != nullptr && client->RawKeyboardInput(key,scanCode,state,mods,magnitude) == false)
		return;
	if(key == GLFW::Key::Escape) // Escape key is hardcoded
	{
		if(client != nullptr)
		{
			if(state == GLFW::KeyState::Press)
				client->ToggleMainMenu();
			return;
		}
	}
	auto buttonState = state;
	auto bValidButtonInput = GetInputButtonState(magnitude,mods,buttonState);
	if(bValidButtonInput == true)
	{
		if(WGUI::GetInstance().HandleKeyboardInput(window,key,scanCode,buttonState,mods))
			return;
	}
	if(client != nullptr && client->KeyboardInput(key,scanCode,state,mods,magnitude) == false)
		return;
	key = static_cast<GLFW::Key>(std::tolower(static_cast<int>(key)));
	Input(static_cast<int>(key),state,buttonState,mods,magnitude);
}
void CEngine::CharInput(GLFW::Window &window,unsigned int c)
{
	if(client != nullptr && client->RawCharInput(c) == false)
		return;
	if(WGUI::GetInstance().HandleCharInput(window,c))
		return;
	if(client != nullptr && client->CharInput(c) == false)
		return;
}
void CEngine::ScrollInput(GLFW::Window &window,Vector2 offset)
{
	if(client != nullptr && client->RawScrollInput(offset) == false)
		return;
	if(WGUI::GetInstance().HandleScrollInput(window,offset))
		return;
	if(client != nullptr && client->ScrollInput(offset) == false)
		return;
	if(offset.y >= 0.f)
	{
		Input(GLFW_CUSTOM_KEY_SCRL_UP,GLFW::KeyState::Press);
		Input(GLFW_CUSTOM_KEY_SCRL_UP,GLFW::KeyState::Release);
	}
	else
	{
		Input(GLFW_CUSTOM_KEY_SCRL_DOWN,GLFW::KeyState::Press);
		Input(GLFW_CUSTOM_KEY_SCRL_DOWN,GLFW::KeyState::Release);
	}
}

void CEngine::OnWindowFocusChanged(GLFW::Window &window,bool bFocused)
{
	m_bWindowFocused = bFocused;
	if(client != nullptr)
		client->UpdateSoundVolume();
}
void CEngine::OnFilesDropped(GLFW::Window &window,std::vector<std::string> &files)
{
	if(client == nullptr)
		return;
	client->OnFilesDropped(files);
}
bool CEngine::IsWindowFocused() const {return m_bWindowFocused;}
void CEngine::LoadMap(const char *cmap)
{
	Engine::LoadMap(cmap);
	/*if(client == nullptr)
		return;
	auto *pMenu = client->GetMainMenu();
	if(pMenu == nullptr)
		return;
	pMenu->OpenLoadScreen();*/
}

bool CEngine::Initialize(int argc,char *argv[])
{
	Engine::Initialize(argc,argv,false);

	auto &cmds = *m_preloadedConfig.get();
	auto res = cmds.find("cl_render_resolution");
	pragma::RenderContext::CreateInfo contextCreateInfo {};
	contextCreateInfo.width = 1024;
	contextCreateInfo.height = 768;
	if(res != nullptr && !res->argv.empty())
	{
		std::vector<std::string> vals;
		ustring::explode(res->argv[0],"x",vals);
		if(vals.size() >= 2)
		{
			contextCreateInfo.width = util::to_int(vals[0]);
			contextCreateInfo.height = util::to_int(vals[1]);
		}
	}
	SetResolution(Vector2i(contextCreateInfo.width,contextCreateInfo.height));
	res = cmds.find("cl_render_window_mode");
	int mode = 0;
	if(res != nullptr && !res->argv.empty())
		mode = util::to_int(res->argv[0]);
	if(mode == 0)
		SetWindowedMode(false);
	else
		SetWindowedMode(true);
	SetNoBorder((mode == 2) ? true : false);

	res = cmds.find("cl_render_monitor");
	if(res != nullptr && !res->argv.empty())
	{
		auto monitor = util::to_int(res->argv[0]);
		auto monitors = GLFW::get_monitors();
		if(monitor < monitors.size() && monitor > 0)
			SetMonitor(monitors[monitor]);
	}

	res = cmds.find("cl_gpu_device");
	if(res != nullptr && !res->argv.empty())
	{
		auto device = res->argv[0];
		std::vector<std::string> subStrings;
		ustring::explode(device,",",subStrings);
		if(subStrings.size() >= 2)
			contextCreateInfo.device = {static_cast<prosper::Vendor>(util::to_uint(subStrings.at(0))),util::to_uint(subStrings.at(1))};
	}

	auto presentMode = Anvil::PresentModeKHR::MAILBOX_KHR;
	res = cmds.find("cl_render_present_mode");
	if(res != nullptr && !res->argv.empty())
	{
		auto mode = util::to_int(res->argv[0]);
		if(mode == 0)
			presentMode = Anvil::PresentModeKHR::IMMEDIATE_KHR;
		else if(mode == 1)
			presentMode = Anvil::PresentModeKHR::FIFO_KHR;
		else
			presentMode = Anvil::PresentModeKHR::MAILBOX_KHR;
	}
	ChangePresentMode(presentMode);

	// Initialize Window context
	pragma::RenderContext::Initialize(contextCreateInfo);

	auto &shaderManager = GetShaderManager();
	shaderManager.RegisterShader("clear_color",[](prosper::Context &context,const std::string &identifier) {return new pragma::ShaderClearColor(context,identifier);});
	shaderManager.RegisterShader("gradient",[](prosper::Context &context,const std::string &identifier) {return new pragma::ShaderGradient(context,identifier);});

	// Initialize Client Instance
	auto matManager = std::make_shared<CMaterialManager>(*this);
	matManager->GetTextureManager().SetTextureFileHandler([this](const std::string &fpath) -> VFilePtr {
		if(FileManager::Exists(fpath) == false)
		{
			static auto formats = get_perfered_image_format_order();
			auto *cl = GetClientState();
			auto path = fpath;
			ufile::remove_extension_from_filename(path);
			for(auto &format : formats)
			{
				if(util::port_file(cl,path +format.extension) == true)
					break;
			}
		}
		return nullptr;
	});
	auto *matErr = matManager->Load("error",nullptr,nullptr,false,nullptr,true);
	m_clInstance = std::unique_ptr<StateInstance>(new StateInstance{matManager,matErr});
	//

	auto &gui = WGUI::Open(*this,matManager);
	auto r = gui.Initialize();
	if(r != WGUI::ResultCode::Ok)
	{
		Con::cerr<<"ERROR: Unable to initialize GUI library: ";
		switch(r)
		{
			case WGUI::ResultCode::UnableToInitializeFontManager:
				Con::cerr<<"Error initializing font manager!";
				break;
			case WGUI::ResultCode::ErrorInitializingShaders:
				Con::cerr<<"Error initializing shaders!";
				break;
			case WGUI::ResultCode::FontNotFound:
				Con::cerr<<"Font not found!";
				break;
			default:
				Con::cout<<"Unknown error!";
				break;
		}
		matManager = nullptr;
		Close();
		Release();
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return false;
	}

	m_speedCam = 1600.0f;
	m_speedCamMouse = 0.2f;
	
	InitializeStagingTarget();
	m_gpuTimerManager = std::make_unique<CSciGPUTimerManager>();

	InitializeSoundEngine();

	auto *cl = OpenClientState();
	RunLaunchCommands();
	if(cl != nullptr)
		SetHRTFEnabled(cl->GetConVarBool("cl_audio_hrtf_enabled"));

#ifdef _WIN32
	if(IsValidationEnabled())
	{
		if(util::is_process_running("bdcam.exe"))
		{
			auto r = MessageBox(NULL,"Bandicam is running and vulkan validation mode is enabled. This is NOT recommended, as Bandicam will cause misleading validation errors! Press OK to continue anyway.","Validation Warning",MB_OK | MB_OKCANCEL);
			if(r == IDCANCEL)
				ShutDown();
		}
	}
#endif
	return true;
}
void CEngine::OnWindowInitialized()
{
	pragma::RenderContext::OnWindowInitialized();

	auto &window = GetWindow();
	window.SetKeyCallback(std::bind(&CEngine::KeyboardInput,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5,1.f));
	window.SetMouseButtonCallback(std::bind(&CEngine::MouseInput,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4));
	window.SetCharCallback(std::bind(&CEngine::CharInput,this,std::placeholders::_1,std::placeholders::_2));
	window.SetScrollCallback(std::bind(&CEngine::ScrollInput,this,std::placeholders::_1,std::placeholders::_2));
	window.SetFocusCallback(std::bind(&CEngine::OnWindowFocusChanged,this,std::placeholders::_1,std::placeholders::_2));
	window.SetDropCallback(std::bind(&CEngine::OnFilesDropped,this,std::placeholders::_1,std::placeholders::_2));
}
void CEngine::InitializeExternalArchiveManager() {util::initialize_external_archive_manager(GetClientState());}
bool CEngine::GetControllersEnabled() const {return m_bControllersEnabled;}
void CEngine::SetControllersEnabled(bool b)
{
	if(m_bControllersEnabled == b)
		return;
	m_bControllersEnabled = b;
	if(b == false)
	{
		GLFW::set_joysticks_enabled(false);
		return;
	}
	GLFW::set_joysticks_enabled(true);
	GLFW::set_joystick_axis_threshold(0.01f);
	GLFW::set_joystick_button_callback([this](const GLFW::Joystick &joystick,uint32_t key,GLFW::KeyState oldState,GLFW::KeyState newState) {
		auto keyOffset = GLFW_CUSTOM_KEY_JOYSTICK_0_KEY_START +joystick.GetJoystickId() *GLFW_CUSTOM_KEY_JOYSTICK_CONTROL_COUNT;
		JoystickButtonInput(GetWindow(),joystick,key +keyOffset,newState);
	});
	GLFW::set_joystick_axis_callback([this](const GLFW::Joystick &joystick,uint32_t axisId,float oldVal,float newVal) {
		m_rawInputJoystickMagnitude = newVal;
		auto mods = GLFW::Modifier::None;
		auto axisOffset = GLFW_CUSTOM_KEY_JOYSTICK_0_AXIS_START +joystick.GetJoystickId() *GLFW_CUSTOM_KEY_JOYSTICK_CONTROL_COUNT;
		if(SEPARATE_JOYSTICK_AXES == true)
		{
			axisId *= 2;
			if(umath::sign(oldVal) != umath::sign(newVal))
			{
				auto prevAxisId = axisId;
				auto prevMods = mods;
				if(oldVal < 0.f)
				{
					++prevAxisId;
					prevMods |= GLFW::Modifier::AxisNegative;
				}
				JoystickAxisInput(GetWindow(),joystick,prevAxisId +axisOffset,prevMods,0.f,0.f -oldVal);
				oldVal = 0.f;
			}
			if(newVal < 0.f)
			{
				oldVal = -oldVal;
				newVal = -newVal;
				++axisId;
				mods |= GLFW::Modifier::AxisNegative;
			}
		}
		JoystickAxisInput(GetWindow(),joystick,axisId +axisOffset,mods,newVal,newVal -oldVal);
	});
	GLFW::set_joystick_state_callback([this](const GLFW::Joystick &joystick,bool bConnected) {
		c_engine->CallCallbacks<void,std::reference_wrapper<const GLFW::Joystick>,bool>("OnJoystickStateChanged",std::ref(joystick),bConnected);
	});
}
REGISTER_CONVAR_CALLBACK_CL(cl_controller_enabled,[](NetworkState *state,ConVar *cv,bool oldVal,bool newVal) {
	c_engine->SetControllersEnabled(newVal);
});

float CEngine::GetRawJoystickAxisMagnitude() const {return m_rawInputJoystickMagnitude;}

Engine::StateInstance &CEngine::GetClientStateInstance() {return *m_clInstance;}

::util::WeakHandle<prosper::Shader> CEngine::ReloadShader(const std::string &name)
{
#ifdef _DEBUG
	bReload = true;
#endif
/*	Con::cerr<<"Loading shader "<<name<<"..."<<Con::endl;
#ifndef _DEBUG
#error ""
#endif*/
	WaitIdle();
	auto whShader = GetShader(name);
	if(whShader.expired())
	{
		if(IsVerbose())
			Con::cwar<<"WARNING: No shader found with name '"<<name<<"'!"<<Con::endl;
		return {};
	}
	if(IsVerbose() == true)
		Con::cout<<"Reloading shader "<<name<<"..."<<Con::endl;
	whShader.get()->Initialize(true);
	return whShader;
}
void CEngine::ReloadShaderPipelines()
{
	WaitIdle();
	if(IsVerbose() == true)
		Con::cout<<"Reloading shaders"<<Con::endl;
	auto &shaderManager = GetShaderManager();
	auto &shaders = shaderManager.GetShaders();
	for(auto &pair : shaders)
		pair.second->Initialize(true);
}

CEngine::~CEngine() {}

extern DLLCLIENT ClientState *client;
void CEngine::HandleLocalPlayerClientPacket(NetPacket &p)
{
	if(client == nullptr)
		return;
	client->HandlePacket(p);
}

void CEngine::Connect(const std::string &ip,const std::string &port)
{
	auto *cl = static_cast<ClientState*>(GetClientState());
	if(cl == NULL)
		return;
	cl->Disconnect();
	c_engine->CloseServerState();
	cl->Connect(ip,port);
}

void CEngine::Disconnect()
{
	auto *cl = static_cast<ClientState*>(GetClientState());
	if(cl == nullptr)
		return;
	if(cl->IsGameActive())
	{
		cl->Disconnect();
		OpenServerState();
	}
	cl->OpenMainMenu();
}

Lua::Interface *CEngine::GetLuaInterface(lua_State *l)
{
	auto *cl = static_cast<ClientState*>(GetClientState());
	if(cl != nullptr)
	{
		if(cl->GetGUILuaState() == l)
			return &cl->GetGUILuaInterface();
		auto *cg = cl->GetGameState();
		if(cg != nullptr && cg->GetLuaState() == l)
			return &cg->GetLuaInterface();
	}
	return Engine::GetLuaInterface(l);
}

NetworkState *CEngine::GetNetworkState(lua_State *l)
{
	auto *cl = static_cast<ClientState*>(GetClientState());
	if(cl == NULL)
		return NULL;
	if(cl->GetLuaState() == l || cl->GetGUILuaState() == l)
		return cl;
	return Engine::GetNetworkState(l);
}

void CEngine::Start()
{
	Engine::Start();
}

void CEngine::Close()
{
	CloseClientState();
	m_auxEffects.clear();
	CloseSoundEngine(); // Has to be closed after client state (since clientstate may still have some references at this point)
	m_clInstance = nullptr;
	WGUI::Close(); // Has to be closed after client state
	c_engine = nullptr;

	// Clear all Vulkan resources before closing the context
	m_stagingRenderTarget = nullptr;

	pragma::CRenderComponent::ClearBuffers();
	pragma::CLightComponent::ClearBuffers();
	CModelSubMesh::ClearBuffers();
	pragma::CParticleSystemComponent::ClearBuffers();

	Engine::Close();
}

void CEngine::UpdateFPS(float t)
{
	const auto weightRatio = 0.8f;
	m_tFPSTime = t *(1.f -weightRatio) +m_tFPSTime *weightRatio;
	if(m_tFPSTime > 0.f)
		m_fps = CUInt32(1.f /m_tFPSTime);
}

static CVar cvProfiling = GetEngineConVar("debug_profiling_enabled");
void CEngine::DrawFrame(prosper::PrimaryCommandBuffer &drawCmd,uint32_t n_current_swapchain_image)
{
	auto ptrDrawCmd = std::static_pointer_cast<prosper::PrimaryCommandBuffer>(drawCmd.shared_from_this());
	CallCallbacks<void,std::reference_wrapper<std::shared_ptr<prosper::PrimaryCommandBuffer>>>("DrawFrame",std::ref(ptrDrawCmd));

	static_cast<CMaterialManager&>(*m_clInstance->materialManager).Update(); // Requires active command buffer
#if DEBUG_TIMER_ENABLED == 1
	CPUTimer t;
	t.Begin();
#endif
#if DEBUG_TIMER_ENABLED == 1
	t.End();
	std::cout<<"#1: "<<t.GetDeltaTimeMs()<<std::endl;
#endif
#if DEBUG_TIMER_ENABLED == 1
	t.End();
	std::cout<<"#2: "<<t.GetDeltaTimeMs()<<std::endl;
#endif
	auto &gui = WGUI::GetInstance();
	gui.Think();

	auto &stagingRt = m_stagingRenderTarget;
	if(m_bFirstFrame == true)
		m_bFirstFrame = false;
	else
	{
		prosper::util::record_image_barrier(
			*drawCmd,stagingRt->GetTexture()->GetImage()->GetAnvilImage(),
			Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL
		);
	}

#if 0
	auto &drawCmdPrimary = static_cast<Anvil::PrimaryCommandBuffer&>(*drawCmd);
	drawCmdPrimary.record_begin_render_pass(
		1, /* in_n_clear_values */
		&attachment_clear_value,
		m_fbos[n_current_swapchain_image].get(),
		render_area,
		m_renderPass.get(),
		VK_SUBPASS_CONTENTS_INLINE
	);
#endif

	DrawScene(ptrDrawCmd,stagingRt);

	auto &finalImg = *stagingRt->GetTexture()->GetImage();
	prosper::util::record_image_barrier(
		*drawCmd,finalImg.GetAnvilImage(),
		Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL
	);

	// Change swapchain image layout to TransferDst
	prosper::util::ImageSubresourceRange subresourceRange {0,1,0,1};
	auto *universal_queue_ptr = m_devicePtr->get_universal_queue(0);

	{

		prosper::util::ImageBarrierInfo imgBarrierInfo {};
		imgBarrierInfo.srcAccessMask = Anvil::AccessFlagBits{};
		imgBarrierInfo.dstAccessMask = Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
		imgBarrierInfo.oldLayout = Anvil::ImageLayout::UNDEFINED;
		imgBarrierInfo.newLayout = Anvil::ImageLayout::TRANSFER_DST_OPTIMAL;
		imgBarrierInfo.subresourceRange = subresourceRange;
		imgBarrierInfo.srcQueueFamilyIndex = imgBarrierInfo.dstQueueFamilyIndex = universal_queue_ptr->get_queue_family_index();

		prosper::util::PipelineBarrierInfo barrierInfo {};
		barrierInfo.srcStageMask = Anvil::PipelineStageFlagBits::TOP_OF_PIPE_BIT;
		barrierInfo.dstStageMask = Anvil::PipelineStageFlagBits::TRANSFER_BIT;
		barrierInfo.imageBarriers.push_back(prosper::util::create_image_barrier(*m_swapchainPtr->get_image(n_current_swapchain_image),imgBarrierInfo));
		prosper::util::record_pipeline_barrier(*drawCmd,barrierInfo);

		// Obsolete (Replaced by code above)
		/*Anvil::ImageBarrier image_barrier(
			0, // source_access_mask
			VK_ACCESS_TRANSFER_WRITE_BIT,
			false,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			universal_queue_ptr->get_queue_family_index(),
			universal_queue_ptr->get_queue_family_index(),
			m_swapchainPtr->get_image(n_current_swapchain_image),
			subresource_range
		);

		drawCmd->record_pipeline_barrier(
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_FALSE, // in_by_region
			0, // in_memory_barrier_count
			nullptr, // in_memory_barrier_ptrs
			0, // in_buffer_memory_barrier_count
			nullptr, // in_buffer_memory_barrier_ptrs
			1, // in_image_memory_barrier_count
			&image_barrier
		);*/
	}

	prosper::util::record_blit_image(*drawCmd,{},finalImg.GetAnvilImage(),*m_swapchainPtr->get_image(n_current_swapchain_image));

	/* Change the swap-chain image's layout to presentable */
	{
		prosper::util::ImageBarrierInfo imgBarrierInfo {};
		imgBarrierInfo.srcAccessMask = Anvil::AccessFlagBits::TRANSFER_WRITE_BIT;
		imgBarrierInfo.dstAccessMask = Anvil::AccessFlagBits::MEMORY_READ_BIT;
		imgBarrierInfo.oldLayout = Anvil::ImageLayout::TRANSFER_DST_OPTIMAL;
		imgBarrierInfo.newLayout = Anvil::ImageLayout::PRESENT_SRC_KHR;
		imgBarrierInfo.subresourceRange = subresourceRange;
		imgBarrierInfo.srcQueueFamilyIndex = imgBarrierInfo.dstQueueFamilyIndex = universal_queue_ptr->get_queue_family_index();

		prosper::util::PipelineBarrierInfo barrierInfo {};
		barrierInfo.srcStageMask = Anvil::PipelineStageFlagBits::TRANSFER_BIT;
		barrierInfo.dstStageMask = Anvil::PipelineStageFlagBits::ALL_COMMANDS_BIT;
		barrierInfo.imageBarriers.push_back(prosper::util::create_image_barrier(*m_swapchainPtr->get_image(n_current_swapchain_image),imgBarrierInfo));
		prosper::util::record_pipeline_barrier(*drawCmd,barrierInfo);

		// Obsolete (Replaced by code above)
		/*Anvil::ImageBarrier image_barrier(
			VK_ACCESS_TRANSFER_WRITE_BIT, // source_access_mask
			VK_ACCESS_MEMORY_READ_BIT, // destination_access_mask
			false,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // old_image_layout
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // new_image_layout
			universal_queue_ptr->get_queue_family_index(),
			universal_queue_ptr->get_queue_family_index(),
			m_swapchainPtr->get_image(n_current_swapchain_image),
			subresource_range
		);

		drawCmd->record_pipeline_barrier(
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_FALSE, // in_by_region
			0, // in_memory_barrier_count
			nullptr, // in_memory_barrier_ptrs
			0, // in_buffer_memory_barrier_count
			nullptr, // in_buffer_memory_barrier_ptrs
			1, // in_image_memory_barrier_count
			&image_barrier
		);*/
	}
	///

#if DEBUG_TIMER_ENABLED == 1
	t.End();
	std::cout<<"#3: "<<t.GetDeltaTimeMs()<<std::endl;
#endif

#if DEBUG_TIMER_ENABLED == 1
	t.End();
	std::cout<<"#4: "<<t.GetDeltaTimeMs()<<std::endl;
#endif
#if DEBUG_TIMER_ENABLED == 1
	t.End();
	std::cout<<"#5: "<<t.GetDeltaTimeMs()<<std::endl;
#endif
}

void CEngine::DrawScene(std::shared_ptr<prosper::PrimaryCommandBuffer> &drawCmd,std::shared_ptr<prosper::RenderTarget> &rt)
{
	auto bProfiling = cvProfiling->GetBool();
	auto *cl = static_cast<ClientState*>(GetClientState());
	auto tStart = std::chrono::steady_clock::now();
	if(cl != nullptr)
	{
		if(bProfiling == true)
			StartStageProfiling(umath::to_integral(ProfilingStage::ClientStateDraw));

		cl->Render(drawCmd,rt);

		if(bProfiling == true)
			EndStageProfiling(umath::to_integral(ProfilingStage::ClientStateDraw));
	}
	auto tEnd = std::chrono::steady_clock::now();
	auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd -tStart);


	//std::cout<<"Duration in ms: "<<dur.count()<<std::endl; // Vulkan TODO
	/*auto w = framebuffer->GetWidth();
	auto h = framebuffer->GetHeight();
	if(bProfiling == true)
		StartStageProfiling(umath::to_integral(ProfilingStage::GUIDraw));
	drawCmd->BeginRenderPass(*targetRenderPass,*targetFramebuffer,w,h,1.f);//,vk::SubpassContents::eSecondaryCommandBuffers);
	*/


	CallCallbacks<void,std::reference_wrapper<std::shared_ptr<prosper::PrimaryCommandBuffer>>>("PreDrawGUI",std::ref(drawCmd));
	prosper::util::record_begin_render_pass(*(*drawCmd),*rt);
	if(c_game != nullptr)
		c_game->PreGUIDraw();
	
	StartGPUTimer(GPUTimerEvent::GUI);
		auto &gui = WGUI::GetInstance();
		gui.Draw();
	StopGPUTimer(GPUTimerEvent::GUI);

	prosper::util::record_end_render_pass(*(*drawCmd));
	CallCallbacks<void,std::reference_wrapper<std::shared_ptr<prosper::PrimaryCommandBuffer>>>("PostDrawGUI",std::ref(drawCmd));

	if(bProfiling == true)
		EndStageProfiling(umath::to_integral(ProfilingStage::GUIDraw));

	if(c_game != nullptr)
		c_game->PostGUIDraw();
}

#include <prosper_util.hpp>
#include <buffers/prosper_buffer.hpp>
#include <buffers/prosper_dynamic_resizable_buffer.hpp>
void CEngine::Think()
{
	GLFW::poll_joystick_events();

	auto tNow = std::chrono::high_resolution_clock::now();
	auto tDelta = tNow -m_tLastFrame;
	auto maxFps = GetFPSLimit();
	if(maxFps > 0 && std::chrono::duration_cast<std::chrono::nanoseconds>(tDelta).count() /1'000'000.0 < 1'000.0 /maxFps)
		return;
	m_tDeltaFrameTime = tDelta;
	m_tLastFrame = tNow;
	const auto sToNs = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count());
	UpdateFPS(static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(m_tDeltaFrameTime).count() /sToNs));
#ifdef ENABLE_PERFORMANCE_TIMER
	static unsigned int thinkTimer = PerformanceTimer::InitializeTimer("CEngine::Think");
	PerformanceTimer::StartMeasurement(thinkTimer);
#endif
	auto bProfiling = cvProfiling->GetBool();
	if(bProfiling == true)
		StartStageProfiling(umath::to_integral(ProfilingStage::EngineThink));
	//auto tStart = std::chrono::high_resolution_clock::now();

	Engine::Think();

	auto *cl = GetClientState();
	if(cl != NULL)
	{
		if(bProfiling == true)
			StartStageProfiling(umath::to_integral(ProfilingStage::ClientStateThink));
		cl->Think(); // Draw?
		if(bProfiling == true)
			EndStageProfiling(umath::to_integral(ProfilingStage::ClientStateThink));
	}
	//if(m_stateClient == NULL || !m_stateClient->IsGameActive())
	//	OpenGL::Clear(GL_COLOR_BUFFER_BIT);
	//WGUI::Draw();
	if(bProfiling == true)
		StartStageProfiling(umath::to_integral(ProfilingStage::EngineDraw));

	pragma::RenderContext::DrawFrame();
	CallCallbacks("Draw");
	GLFW::poll_events(); // Needs to be called AFTER rendering!

	if(bProfiling == true)
		EndStageProfiling(umath::to_integral(ProfilingStage::EngineDraw));

	//auto tEnd = std::chrono::high_resolution_clock::now();
	//auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(tEnd -tStart);
	//UpdateFPS(static_cast<float>(dur.count() /1'000'000'000.0));
	EndFrame();
#ifdef ENABLE_PERFORMANCE_TIMER
	PerformanceTimer::EndMeasurement(thinkTimer);
#endif
	if(bProfiling == true)
		EndStageProfiling(umath::to_integral(ProfilingStage::EngineThink));
}

void CEngine::Tick()
{
	auto bProfiling = cvProfiling->GetBool();
	if(bProfiling == true)
		StartStageProfiling(umath::to_integral(ProfilingStage::EngineTick));
#ifdef ENABLE_PERFORMANCE_TIMER
	static unsigned int tickTimer = PerformanceTimer::InitializeTimer("CEngine::Tick");
	PerformanceTimer::StartMeasurement(tickTimer);
#endif
	ProcessConsoleInput();

	// The client tick has to run BEFORE the server tick!!!
	// This is to avoid issues in singleplayer, where the client would use data it received from the server and apply the same calculations on the already modified data.
	auto *cl = GetClientState();
	if(cl != NULL)
	{
		auto bProfiling = cvProfiling->GetBool();
		if(bProfiling == true)
			StartStageProfiling(umath::to_integral(ProfilingStage::ClientStateTick));
		cl->Tick();
		if(bProfiling == true)
			EndStageProfiling(umath::to_integral(ProfilingStage::ClientStateTick));
	}

	auto *sv = GetServerState();
	if(sv != NULL)
	{
		auto bProfiling = cvProfiling->GetBool();
		if(bProfiling == true)
			StartStageProfiling(umath::to_integral(ProfilingStage::ServerStateTick));
		sv->Tick();
		if(bProfiling == true)
			EndStageProfiling(umath::to_integral(ProfilingStage::ServerStateTick));
	}

#ifdef ENABLE_PERFORMANCE_TIMER
	PerformanceTimer::EndMeasurement(tickTimer);
#endif
	if(bProfiling == true)
		EndStageProfiling(umath::to_integral(ProfilingStage::EngineTick));
}

bool CEngine::IsServerOnly() {return false;}

void CEngine::UseFullbrightShader(bool b) {m_bFullbright = b;}

void CEngine::OnResolutionChanged(uint32_t width,uint32_t height)
{
	prosper::Context::OnResolutionChanged(width,height);
	InitializeStagingTarget();

	auto &wgui = WGUI::GetInstance();
	auto *baseEl = wgui.GetBaseElement();
	if(baseEl != nullptr)
		baseEl->SetSize(width,height);

	auto *cl = GetClientState();
	if(cl == nullptr)
		return;
	auto *game = static_cast<CGame*>(cl->GetGameState());
	if(game == nullptr)
		return;
	game->Resize();
}

REGISTER_CONVAR_CALLBACK_CL(cl_render_monitor,[](NetworkState*,ConVar*,int32_t,int32_t monitor) {
	auto monitors = GLFW::get_monitors();
	if(monitor < monitors.size() && monitor >= 0)
		c_engine->SetMonitor(monitors[monitor]);
})

REGISTER_CONVAR_CALLBACK_CL(cl_render_window_mode,[](NetworkState*,ConVar*,int32_t,int32_t val) {
	c_engine->SetWindowedMode(val != 0);
	c_engine->SetNoBorder(val == 2);
})

REGISTER_CONVAR_CALLBACK_CL(cl_render_resolution,[](NetworkState*,ConVar*,std::string,std::string val) {
	std::vector<std::string> vals;
	ustring::explode(val,"x",vals);
	if(vals.size() < 2)
		return;
	auto x = util::to_int(vals[0]);
	auto y = util::to_int(vals[1]);
	Vector2i resolution(x,y);
	c_engine->SetResolution(resolution);
	auto *client = static_cast<ClientState*>(c_engine->GetClientState());
	if(client == nullptr)
		return;
	auto &wgui = WGUI::GetInstance();
	auto *el = wgui.GetBaseElement();
	if(el == nullptr)
		return;
	el->SetSize(resolution);
	auto *menu = client->GetMainMenu();
	if(menu == nullptr)
		return;
	menu->SetSize(x,y);
})