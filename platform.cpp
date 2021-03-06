#include <SDL.h>
#include "assert.h"

#include "timer.h"
#include "timer.cpp"

#include "boundry.h"
#include "platform.h"

#include "circular_buffer.h" 

//Files
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

//Memory
#include <sys/mman.h>

#include "ini.h"

internal bool32
InitPlatform(platform_state *Platform)
{
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		printf("Platform: SDL Initialization error: %s\n", SDL_GetError());
		return false;
	}
	if ((Platform->Window = SDL_CreateWindow("Fakebird", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE)) == NULL) {
		printf("Platform: Window creation error: %s\n", SDL_GetError());
		return false;
	}
	if ((Platform->Renderer = SDL_CreateRenderer(Platform->Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)) == NULL) {
		printf("Platform: Renderer creation error: %s\n", SDL_GetError());
		return false;
	}
	Platform->Running = true;

	Platform->OffscreenBuffer.Texture = SDL_CreateTexture(Platform->Renderer,
											SDL_PIXELFORMAT_RGBA8888,
											SDL_TEXTUREACCESS_STREAMING,
											SCREEN_WIDTH,
											SCREEN_HEIGHT);
	if (Platform->OffscreenBuffer.Texture == NULL){
		printf("Platform: Offscreen Buffer texture creation error: %s\n", SDL_GetError());
		return false;
	}
	Platform->OffscreenBuffer.BytesPerPixel = sizeof(uint32);
	Platform->OffscreenBuffer.Width = SCREEN_WIDTH;
	Platform->OffscreenBuffer.Height = SCREEN_HEIGHT;
	Platform->OffscreenBuffer.Pitch = SCREEN_WIDTH * sizeof(uint32);
	Platform->OffscreenBuffer.Memory = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32));
	assert(Platform->OffscreenBuffer.Memory);
	return true;
}

uint32 SafeTruncateUint64(Uint64 Value){
	assert(Value <= 0xffffffff);
	uint32 Result = (uint32)Value;
	return Result;
}

internal void
ResizeOffscreenBuffer(SDL_Renderer *Renderer, offscreen_buffer *Buffer, int32 Width, int32 Height){
	if (Buffer->Memory){
		free(Buffer->Memory);
	}
	if (Buffer->Texture){
		SDL_DestroyTexture(Buffer->Texture);
	}
	Buffer->Texture = SDL_CreateTexture(Renderer,
								SDL_PIXELFORMAT_RGBA8888,
								SDL_TEXTUREACCESS_STREAMING,
								Width,
								Height);
	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = sizeof(uint32);
	Buffer->Pitch = Buffer->Width * int32(sizeof(uint32));
	Buffer->Memory = malloc(Buffer->Width*Buffer->Height*sizeof(uint32));
}


debug_read_file_result
DEBUGPlatformReadEntireFile(char *FileName){
	debug_read_file_result Result = {};
	int FileHandle = open(FileName, O_RDONLY);
	if(FileHandle == -1){
		return Result;
	}

	struct stat FileStatus;
	if (fstat(FileHandle, &FileStatus) == -1){
		close(FileHandle);
		return Result;
	}
	Result.ContentsSize = SafeTruncateUint64(FileStatus.st_size);

	Result.Contents = malloc(Result.ContentsSize);
	if(!Result.Contents){
		Result.ContentsSize = 0;
		close(FileHandle);
		return Result;
	}
	
	uint64 BytesStoread = Result.ContentsSize;
	uint8 *NextByteLocation = (uint8*)Result.Contents;
	while(BytesStoread){
		int64 BytesRead = read(FileHandle, NextByteLocation, BytesStoread);
		if(BytesRead == -1 ){
			free(Result.Contents);
			Result.Contents = 0;
			Result.ContentsSize = 0;
			close(FileHandle);
			return Result;
		}
		BytesStoread -= BytesRead;
		NextByteLocation += BytesRead;
	}
	close(FileHandle);
	return Result;
}

loaded_bitmap
DEBUGPlatformLoadBitmapFromFile(char *FileName){
	loaded_bitmap Result = {};
	SDL_Surface *ImageSurface = SDL_LoadBMP(FileName);
	if(ImageSurface){
		Result.Texels = ImageSurface->pixels;
		Result.Width = ImageSurface->w;
		Result.Height = ImageSurface->h;

		assert(Result.Width > 0 && Result.Height > 0);
	}else{
		printf("Platform: Window creation error: %s\n", SDL_GetError());
	}

	return Result;
}

void
DEBUGPlatformFreeFileMemory(debug_read_file_result FileHandle){
	if(FileHandle.ContentsSize){
		free(FileHandle.Contents);
	}
}

bool32
DEBUGPLatformWriteEntireFile(char *Filename, uint64 MemorySize, void *Memory){
	int FileHandle = open(Filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if(FileHandle == -1){
		return false;
	}

	uint64 BytesToWrite = MemorySize;
	uint8 *NextByteLocation = (uint8*)Memory;
	while(BytesToWrite){
		int64 BytesWritten = write(FileHandle, NextByteLocation, BytesToWrite);
		if(BytesWritten == -1){
			close(FileHandle);
			return false;
		}
		BytesToWrite -= BytesWritten;
		NextByteLocation += BytesWritten;
	}
	close(FileHandle);
	return true;
}

internal void
AllocateGameMemory(game_memory *GameMemory) {
	//GameMemory->Size = Kilobytes(200);
	GameMemory->Size = Kilobytes(500);
	GameMemory->BaseAddress = (void*)Gigabytes(10);
	//LPVOID BaseAddress = (LPVOID*)Gigabytes(10);
	GameMemory->BaseAddress = mmap(GameMemory->BaseAddress, GameMemory->Size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	//GameMemory->BaseAddress = VirtualAlloc(BaseAddress, GameMemory->Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	GameMemory->PlatformServices.ReadEntireFile = DEBUGPlatformReadEntireFile;
	GameMemory->PlatformServices.FreeFileMemory = DEBUGPlatformFreeFileMemory;
	GameMemory->PlatformServices.WriteEntireFile = DEBUGPLatformWriteEntireFile;
	GameMemory->PlatformServices.LoadBitmapFromFile = DEBUGPlatformLoadBitmapFromFile;
}

internal bool32
ProcessInput(game_input *OldInput, game_input *NewInput, SDL_Event* Event, platform_state *Platform)
{
	*NewInput = *OldInput;
	while (SDL_PollEvent(Event) != 0)
	{
		switch (Event->type) {
		case SDL_QUIT:
			return false;
		case SDL_KEYDOWN:
			if (Event->key.keysym.sym == SDLK_ESCAPE) {
				NewInput->Escape.EndedDown = true;
				return false;
			} else if (Event->key.keysym.sym == SDLK_SPACE) {
				NewInput->Space.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_LCTRL) {
				NewInput->LeftCtrl.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_e) {
				NewInput->e.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_g) {
				NewInput->g.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_p) {
				NewInput->p.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_r) {
				NewInput->r.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_s) {
				NewInput->s.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_t) {
				NewInput->t.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_m) {
				NewInput->m.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_n) {
				NewInput->n.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_o) {
				NewInput->o.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_UP) {
				NewInput->ArrowUp.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_DOWN) {
				NewInput->ArrowDown.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_RIGHT) {
				NewInput->ArrowRight.EndedDown = true;
			} else if (Event->key.keysym.sym == SDLK_LEFT) {
				NewInput->ArrowLeft.EndedDown = true;
			}
			break;
		case SDL_KEYUP:
			if (Event->key.keysym.sym == SDLK_SPACE) {
				NewInput->Space.EndedDown = false;
			}else if (Event->key.keysym.sym == SDLK_LCTRL) {
				NewInput->LeftCtrl.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_e) {
				NewInput->e.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_g) {
				NewInput->g.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_p) {
				NewInput->p.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_r) {
				NewInput->r.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_s) {
				NewInput->s.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_t) {
				NewInput->t.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_m) {
				NewInput->m.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_n) {
				NewInput->n.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_o) {
				NewInput->o.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_UP) {
				NewInput->ArrowUp.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_DOWN) {
				NewInput->ArrowDown.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_RIGHT) {
				NewInput->ArrowRight.EndedDown = false;
			} else if (Event->key.keysym.sym == SDLK_LEFT) {
				NewInput->ArrowLeft.EndedDown = false;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (Event->button.button == SDL_BUTTON_LEFT) {
				NewInput->MouseLeft.EndedDown = true;
			} else if (Event->button.button == SDL_BUTTON_RIGHT) {
				NewInput->MouseRight.EndedDown = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (Event->button.button == SDL_BUTTON_LEFT) {
				NewInput->MouseLeft.EndedDown = false;
			} else if (Event->button.button == SDL_BUTTON_RIGHT) {
				NewInput->MouseRight.EndedDown = false;
			}
			break;
		case SDL_WINDOWEVENT:
			if (Event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				int32 NewWidth, NewHeight;
				SDL_Window *Window = SDL_GetWindowFromID(Event->window.windowID); 
				SDL_GetWindowSize(Window, &NewWidth, &NewHeight);
				ResizeOffscreenBuffer(Platform->Renderer, &Platform->OffscreenBuffer, NewWidth, NewHeight);
			}
			break;
		}
	}
	SDL_GetMouseState(&NewInput->MouseX, &NewInput->MouseY);

	for (uint32 Index = 0; Index < sizeof(NewInput->Buttons)/sizeof(game_button_state); Index++) {
		NewInput->Buttons[Index].Changed = (OldInput->Buttons[Index].EndedDown ==
			NewInput->Buttons[Index].EndedDown) ? false : true;
	}
	return true;
}

internal void
CleanUp(platform_state *Platform)
{
	SDL_DestroyWindow(Platform->Window);
	Platform->Window = nullptr;

	SDL_DestroyRenderer(Platform->Renderer);
	Platform->Renderer = nullptr;

	SDL_DestroyTexture(Platform->OffscreenBuffer.Texture);
	Platform->OffscreenBuffer.Texture = nullptr;

	free(Platform->OffscreenBuffer.Memory);

	SDL_Quit();
}

struct loaded_game_code{
	void *LibraryHandle;
	game_update_and_render *UpdateAndRender;
	struct timespec LastLoadTime;
	bool IsValid;
};

internal loaded_game_code
LoadGameCode(){
	loaded_game_code Result = {};
	Result.LibraryHandle = SDL_LoadObject("./game.so");
	if(Result.LibraryHandle){
		Result.UpdateAndRender = (game_update_and_render*)SDL_LoadFunction(Result.LibraryHandle , "GameUpdateAndRender");
	}else{
		printf("shared library load fail: %s\n", SDL_GetError());
	}

	Result.IsValid = (Result.UpdateAndRender);
	if(!Result.IsValid){
		printf("function name load fail: %s\n", SDL_GetError());
		Result.UpdateAndRender = GameUpdateAndRenderStub;
	}

	return Result;
}

internal void
UnloadGameCode(loaded_game_code GameCode){
	if(GameCode.LibraryHandle){
		SDL_UnloadObject(GameCode.LibraryHandle);
		GameCode.UpdateAndRender = GameUpdateAndRenderStub;
	}
}

int main(int Count, char *Arguments[])
{
	platform_state Platform = {};
	assert(InitPlatform(&Platform));

	game_memory GameMemory = {};
	AllocateGameMemory(&GameMemory);
	assert(GameMemory.BaseAddress && GameMemory.Size);

	playback_buffer PlaybackBuffer = NewPlaybackBuffer(32, 32, SafeTruncateUint64(GameMemory.Size));
	printf("Memory BaseAddress: %lu, GameMemory.Size: %lu KB\n",
			(uint64)GameMemory.BaseAddress, GameMemory.Size / (uint64)1e3);

	game_input OldInput = {};
	game_input NewInput = {};
	loaded_game_code GameCode = {};

	while (Platform.Running)
	{
		{
			struct stat CodeStatus = {};
			stat("./game.so", &CodeStatus);
			if ((CodeStatus.st_mtim.tv_sec > GameCode.LastLoadTime.tv_sec) ||
				(CodeStatus.st_mtim.tv_sec == GameCode.LastLoadTime.tv_sec &&
				 CodeStatus.st_mtim.tv_nsec > GameCode.LastLoadTime.tv_nsec)){
				printf("Loading game code, OldTime: %lds, NewTime: %lds\n",
						GameCode.LastLoadTime.tv_sec, CodeStatus.st_mtim.tv_sec);
				UnloadGameCode(GameCode);
				GameCode = LoadGameCode();
				GameCode.LastLoadTime = CodeStatus.st_mtim;
			}
		}


		Platform.FPS.start();
		Platform.Running = ProcessInput(&OldInput, &NewInput, &Platform.Event, &Platform);

		if (NewInput.p.EndedDown && NewInput.p.Changed) {
			Platform.PlaybackStarted = !Platform.PlaybackStarted;
			if (!Platform.PlaybackStarted) {
				ClearPlaybackBuffer(&PlaybackBuffer);
				for(int ButtonIndex = 0; ButtonIndex < 9; ButtonIndex++){
					NewInput.Buttons[ButtonIndex].EndedDown = false;
					NewInput.Buttons[ButtonIndex].Changed = false;
				}
			}
		}

		if (Platform.PlaybackStarted) {
			PeekAndStepPlaybackBuffer(&PlaybackBuffer, &NewInput, GameMemory.BaseAddress, Platform.FrameCount);
		}
		else {
			PushPlaybackBuffer(&PlaybackBuffer, &NewInput, GameMemory.BaseAddress, Platform.FrameCount);
		}

		GameCode.UpdateAndRender(&GameMemory, Platform.OffscreenBuffer, &NewInput);
		SDL_UpdateTexture(Platform.OffscreenBuffer.Texture, 0, Platform.OffscreenBuffer.Memory, Platform.OffscreenBuffer.Pitch);
		SDL_RenderCopy(Platform.Renderer, Platform.OffscreenBuffer.Texture, 0, 0);
		SDL_RenderPresent(Platform.Renderer);
		
		//SDL_Delay(FRAME_DURATION - ((Platform.FPS.get_time() <= FRAME_DURATION) ? Platform.FPS.get_time() : FRAME_DURATION));
		Platform.FPS.update_avg_fps();

#if DEBUG_PROFILING
		printf("fps: %f,\n", Platform.FPS.get_average_fps());
		for(uint32 i = 0; i < ArrayCount(DEBUG_TABLE_NAMES); i++){
			uint64 AverageOpDurationInCyles = (GameMemory.DEBUG_CYCLE_TABLE[i].Calls) ? GameMemory.DEBUG_CYCLE_TABLE[i].CycleCount/GameMemory.DEBUG_CYCLE_TABLE[i].Calls : 0;
			printf("\t%-30s:%15lucy,%10lucy/op,%10.2f,%10lu calls\n",
					DEBUG_TABLE_NAMES[i], GameMemory.DEBUG_CYCLE_TABLE[i].CycleCount, AverageOpDurationInCyles,
					100.0 * (real64)GameMemory.DEBUG_CYCLE_TABLE[i].CycleCount/(real64)GameMemory.DEBUG_CYCLE_TABLE[DEBUG_GameUpdateAndRender].CycleCount,
					GameMemory.DEBUG_CYCLE_TABLE[i].Calls);
		}
		printf("\n");
#endif

		OldInput = NewInput;
		Platform.FrameCount++;
	}
	UnloadGameCode(GameCode);

	DestroyPlaybackBuffer(&PlaybackBuffer);
	CleanUp(&Platform);
	return 0;
}
