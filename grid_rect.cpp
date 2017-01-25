#if !defined(GRID_RECT_CPP)
#define GRID_RECT_CPP

internal inline vec2i
GetGridP(int32 X, int32 Y, rectangle ScreenRect){
	return vec2i{(X-ScreenRect.MinX)/BLOCK_WIDTH_IN_PIXELS, ((ScreenRect.MaxY - ScreenRect.MinY) - (Y-ScreenRect.MinY))/BLOCK_HEIGHT_IN_PIXELS};
}

internal inline rectangle
GetGridRect(int32 X, int32 Y, rectangle ScreenRect){
	return rectangle{X*BLOCK_WIDTH_IN_PIXELS,
					ScreenRect.MaxY- (Y+1)*BLOCK_HEIGHT_IN_PIXELS,
					(X+1)*BLOCK_WIDTH_IN_PIXELS,
					ScreenRect.MaxY - Y*BLOCK_HEIGHT_IN_PIXELS};
}

internal inline rectangle
GetGridRectFromScreenCoords(int32 X, int32 Y, rectangle ScreenRect){
	vec2i  GridP = GetGridP(X, Y, ScreenRect);
	return GetGridRect(GridP.X, GridP.Y, ScreenRect);
}

internal inline vec2i
GetTileGridP(int32 X, int32 Y, rectangle SprieAtlasRect){
	return vec2i{(X-SprieAtlasRect.MinX)/SOURCE_BLOCK_WIDTH_IN_PIXELS, (Y-SprieAtlasRect.MinY)/SOURCE_BLOCK_HEIGHT_IN_PIXELS};
}

internal inline rectangle
GetTileGridRect(int32 X, int32 Y, rectangle SpriteAtlasRect){
	vec2i GridP = GetTileGridP(X, Y, SpriteAtlasRect);
	return rectangle{GridP.X*SOURCE_BLOCK_WIDTH_IN_PIXELS,
					GridP.Y*SOURCE_BLOCK_WIDTH_IN_PIXELS,
					(GridP.X+1)*SOURCE_BLOCK_WIDTH_IN_PIXELS,
					(GridP.Y+1)*SOURCE_BLOCK_HEIGHT_IN_PIXELS};
}

internal rectangle
LerpGridRects(vec2i PosA, vec2i PosB, real32 t, rectangle ScreenRect){
	rectangle Result = GetGridRect(PosA.X, PosA.Y, ScreenRect);
	vec2i Delta = {(int32)(t*(real32)BLOCK_WIDTH_IN_PIXELS*((real32)PosB.X - (real32)PosA.X)),
				   -(int32)(t*(real32)BLOCK_HEIGHT_IN_PIXELS*((real32)PosB.Y - (real32)PosA.Y))};
	Result.MinP += Delta;
	Result.MaxP += Delta;
	return Result;
}
#endif //GRID_RECT_CPP
