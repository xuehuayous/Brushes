
///  \file CZLayer.cpp
///  \brief This is the file implements the class CZLayer.
///
///		
///
///  \version	1.0.0
///	 \author	Charly Zhang<chicboi@hotmail.com>
///  \date		2014-10-15
///  \note

#include "CZLayer.h"
#include "../CZUtil.h"
#include "CZPainting.h"
#include "../graphic/glDef.h"

CZLayer::CZLayer(CZPainting* paiting_) : ptrPainting(paiting_)
{
	visible = true;
	alphaLocked = false;
	locked =  false;
	colorBalance = NULL;
	hueSaturation = NULL;
	clipWhenTransformed = true;
	ptrGLContext = NULL;
	transform = CZAffineTransform::makeIdentity();

	blendMode = kBlendModeNormal;
	opacity = 1.0f;
	isSaved = kSaveStatusUnsaved;
	image = NULL;
	myTexture = NULL;
	hueChromaLuma = NULL;

	if (ptrPainting)
	{
		ptrGLContext = ptrPainting->getGLContext();
		myTexture = ptrPainting->generateTexture(image);
		hueChromaLuma = ptrPainting->generateTexture();
	}

	uuid = CZUtil::generateUUID();

	undoFragment = redoFragment = NULL;

	canRedo = canUndo = false;
}
CZLayer::~CZLayer()
{
	ptrGLContext->setAsCurrent();
	delete myTexture;
	delete hueChromaLuma;

	if(image) { delete image; image = NULL;}
	if(colorBalance) { delete colorBalance; colorBalance = NULL;}
	if(hueSaturation) { delete hueSaturation; hueSaturation = NULL;}
	if (uuid)	{	delete []uuid; uuid = NULL;	}
}

/// 图层的图像数据
CZImage *CZLayer::imageData()
{
	CZRect bounds = ptrPainting->getBounds();
	return imageDataInRect(bounds);
}

/// 生成特定矩形区域的图像数据
CZImage *CZLayer::imageDataInRect(const CZRect &rect)
{
	if(!ptrPainting)
	{
		LOG_ERROR("ptrPainting is null\n");
		return NULL;
	}

	ptrGLContext->setAsCurrent();

	int w = rect.size.width;
	int h = rect.size.height;
	int x = rect.origin.x;
	int y = rect.origin.y;

	CZFbo fbo;
	fbo.setColorRenderBuffer(w, h);

	/// 开始fbo
	fbo.begin();

	CZSize paintingSize = ptrPainting->getDimensions();
	glViewport(0, 0, paintingSize.width, paintingSize.height);

	CZMat4 projMat,transMat;
	projMat.SetOrtho(0,paintingSize.width,0,paintingSize.height,-1.0f,1.0f);
	transMat.SetTranslation(-x, -y, 0);
	projMat = projMat * transMat;

	// use shader program
	CZShader *shader = ptrPainting->getShader("nonPremultipliedBlit");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return NULL;
	}

	shader->begin();

	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"),1,GL_FALSE,projMat);
	glUniform1i(shader->getUniformLocation("texture"), (GLuint) 0);
	glUniform1f(shader->getUniformLocation("opacity"), 1.0f); // fully opaque

	glActiveTexture(GL_TEXTURE0);
	// Bind the texture to be used
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);

	// clear the buffer to get a transparent background
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// set up premultiplied normal blend
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	GL_BIND_VERTEXARRAY(ptrPainting->getQuadVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	GL_BIND_VERTEXARRAY(0);

	shader->end();

	CZImage *ret = fbo.produceImageForCurrentState();

	fbo.end();

	return ret;
}

/// 绘制图层
void CZLayer::basicBlit(CZMat4 &projection)
{
	if (ptrPainting == NULL)
	{
		LOG_ERROR("ptrPainting is NULL\n");
		return;
	}

	// use shader program
	CZShader *shader = ptrPainting->getShader("blit");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}

	shader->begin();

	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"), 1, GL_FALSE, projection);
	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1f(shader->getUniformLocation("opacity"), opacity);

	
	// Bind the texture to be used
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);

	/// 配置绘制模式
	configureBlendMode();

	GL_BIND_VERTEXARRAY(ptrPainting->getQuadVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	GL_BIND_VERTEXARRAY(0);

	shader->end();
}
/// 绘制图层（考虑移动转换、颜色调整等）
void CZLayer::blit(CZMat4 &projection)
{
	if (ptrPainting == NULL)
	{
		LOG_ERROR("ptrPainting is NULL\n");
		return;
	}

	if(!transform.isIdentity())
	{
		blit(projection,transform);
		return;
	}
	
	// use shader program
	CZShader *shader = NULL;
	
	if (colorBalance)
		shader = ptrPainting->getShader("colorBalanceBlit");
	else if(hueSaturation)
		shader = ptrPainting->getShader("blitFromHueChromaLuma");
	else
		shader = ptrPainting->getShader("blit");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}
	
	shader->begin();

	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"), 1, GL_FALSE, projection);
	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1f(shader->getUniformLocation("opacity"), opacity);

	if(colorBalance)
	{
		glUniform1f(shader->getUniformLocation("redShift"), colorBalance->redShift);
		glUniform1f(shader->getUniformLocation("greenShift"), colorBalance->greenShift);
		glUniform1f(shader->getUniformLocation("blueShift"), colorBalance->blueShift);
		glUniform1i(shader->getUniformLocation("premultiply"), 1);
	}
	else if (hueSaturation)
	{
		glUniform1f(shader->getUniformLocation("hueShift"), hueSaturation->hueShift);
		glUniform1f(shader->getUniformLocation("saturationShift"), hueSaturation->saturationShift);
		glUniform1f(shader->getUniformLocation("brightnessShift"), hueSaturation->brightnessShift);
		glUniform1i(shader->getUniformLocation("premultiply"), 1);
	}
	
	// Bind the texture to be used
	glActiveTexture(GL_TEXTURE0);
	if (hueSaturation)
		glBindTexture(GL_TEXTURE_2D, hueChromaLuma->texId);
	else
		glBindTexture(GL_TEXTURE_2D, myTexture->texId);

	/// 配置绘制模式
	configureBlendMode();

	GL_BIND_VERTEXARRAY(ptrPainting->getQuadVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	GL_BIND_VERTEXARRAY(0);

	shader->end();
}
/// 叠加擦除纹理
void CZLayer::blit(CZMat4 &projection, CZTexture *maskTexture)
{
	if (ptrPainting == NULL)
	{
		LOG_ERROR("ptrPainting is NULL\n");
		return;
	}

	if (maskTexture == NULL)
	{
		LOG_ERROR("maskTexture is NULL\n");
		return;
	}

	// use shader program
	CZShader *shader = ptrPainting->getShader("blitWithEraseMask");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}

	shader->begin();
	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"),1,GL_FALSE,projection);
	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1f(shader->getUniformLocation("opacity"), opacity);
	glUniform1i(shader->getUniformLocation("mask"), 1);

	// Bind the texture to be used
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, maskTexture->texId);

	/// 配置绘制模式
	configureBlendMode();

	GL_BIND_VERTEXARRAY(ptrPainting->getQuadVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	GL_BIND_VERTEXARRAY(0);

	shader->end();
}
/// 叠加绘制纹理
void CZLayer::blit(CZMat4 &projection, CZTexture *maskTexture, CZColor &bgColor)
{
	if (ptrPainting == NULL)
	{
		LOG_ERROR("ptrPainting is NULL\n");
		return;
	}

	if (maskTexture == NULL)
	{
		LOG_ERROR("maskTexture is NULL\n");
		return;
	}

	// use shader program
	CZShader *shader = ptrPainting->getShader("blitWithMask");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}

	shader->begin();

	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"), 1, GL_FALSE, projection);
	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1f(shader->getUniformLocation("opacity"), opacity);
	glUniform4f(shader->getUniformLocation("color"), bgColor.red, bgColor.green, bgColor.blue, bgColor.alpha);
	glUniform1i(shader->getUniformLocation("mask"), 1);
	glUniform1i(shader->getUniformLocation("lockAlpha"), alphaLocked);

	// Bind the texture to be used
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, maskTexture->texId);

	/// 配置绘制模式
	configureBlendMode();

	GL_BIND_VERTEXARRAY(ptrPainting->getQuadVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	GL_BIND_VERTEXARRAY(0);

	shader->end();
}

/// 将绘制的笔画合并到当前图层
void CZLayer::commitStroke(CZRect &bounds, CZColor &color, bool erase, bool undoable)
{
	if (undoable) registerUndoInRect(bounds);

	//ptrPainting->beginSuppressingNotifications();

	isSaved = kSaveStatusUnsaved;

	if (ptrPainting == NULL)
	{
		LOG_ERROR("ptrPainting is NULL!\n");
		return;
	}

	ptrGLContext->setAsCurrent();

	CZFbo fbo;
	fbo.setTexture(myTexture);

	fbo.begin();

	CZShader *shader = erase ? ptrPainting->getShader("compositeWithEraseMask") : ptrPainting->getShader("compositeWithMask");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}

	shader->begin();

	CZMat4 projection;
	CZSize size = ptrPainting->getDimensions();
	projection.SetOrtho(0,size.width,0,size.height,-1.0f, 1.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);
	// temporarily turn off linear interpolation to work around "emboss" bug
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ptrPainting->activePaintTexture->texId);

	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1i(shader->getUniformLocation("mask"), 1);
	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"), 1, GL_FALSE, projection);
	glUniform1i(shader->getUniformLocation("lockAlpha"), alphaLocked);

	if (!erase) 
	{
		glUniform4f(shader->getUniformLocation("color"), color.red, color.green, color.blue, color.alpha);
	}

	glBlendFunc(GL_ONE, GL_ZERO);

	GL_BIND_VERTEXARRAY(ptrPainting->getQuadVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	GL_BIND_VERTEXARRAY(0);

	// turn linear interpolation back on
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	CZCheckGLError();
	
	fbo.end();

}

/// 调整颜色
void CZLayer::commitColorAdjustments()
{
	/*
	WDShader *shader = nil;

	if (self.colorBalance) {
		shader = [self.painting getShader:@"colorBalanceBlit"];
	} else if (self.hueSaturation) {
		shader = [self.painting getShader:@"blitFromHueChromaLuma"];
	} else {
		return;
	}

	[self modifyWithBlock:^{
		// handle viewing matrices
		GLfloat proj[16];
		// setup projection matrix (orthographic)
		mat4f_LoadOrtho(0, self.painting.width, 0, self.painting.height, -1.0f, 1.0f, proj);

		glUseProgram(shader.program);

		glActiveTexture(GL_TEXTURE0);
		if (self.hueSaturation) {
			glBindTexture(GL_TEXTURE_2D, self.hueChromaLuma);
		} else {
			glBindTexture(GL_TEXTURE_2D, self.textureName);
		}

		glUniform1i(shader->locationForUniform:@"texture"], 0);
		glUniformMatrix4fv(shader->locationForUniform:@"modelViewProjectionMatrix"], 1, GL_FALSE, proj);

		if (self.colorBalance) {
			glUniform1f(shader->:@"redShift"], colorBalance_.redShift);
			glUniform1f(shader->locationForUniform:@"greenShift"], colorBalance_.greenShift);
			glUniform1f(shader->locationForUniform:@"blueShift"], colorBalance_.blueShift);
			glUniform1i(shader->locationForUniform:@"premultiply"], 0);
		} else {
			glUniform1f(shader->locationForUniform:@"hueShift"], hueSaturation_.hueShift);
			glUniform1f(shader->locationForUniform:@"saturationShift"], hueSaturation_.saturationShift);
			glUniform1f(shader->locationForUniform:@"brightnessShift"], hueSaturation_.brightnessShift);
			glUniform1i(shader->locationForUniform:@"premultiply"], 0);
		}

		glBlendFunc(GL_ONE, GL_ZERO);

		glBindVertexArrayOES(self.painting.quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// unbind VAO
		glBindVertexArrayOES(0);
	} newTexture:NO undoBits:YES];

	self.colorBalance = nil;
	self.hueSaturation = nil;
	*/
}

/// 合并另以图层
bool CZLayer::merge(CZLayer *layer)
{
	return true;
}

/// 将图像经过变换后绘制
void CZLayer::renderImage(CZImage* img, CZAffineTransform &trans)
{
	if (ptrPainting == NULL)
	{
		LOG_ERROR("ptrPainting is NULL\n");
		return;
	}

	if (img == NULL)
	{
		LOG_WARN("image is NULL!\n");
		return;
	}
	
	/// register undo fragment
	CZRect rect(0,0,img->width,img->height);
	registerUndoInRect(trans.applyToRect(rect));

	bool hasAlpha = false;		///< 

	ptrGLContext->setAsCurrent();
	CZTexture *tex = CZTexture::produceFromImage(img);

	CZShader *shader = NULL;
	shader = hasAlpha ? ptrPainting->getShader("unPremultipliedBlit") : ptrPainting->getShader("nonPremultipliedBlit");
	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}

	CZFbo fbo;
	fbo.setTexture(myTexture);

	fbo.begin();

	shader->begin();

	CZMat4 projection;
	CZSize size = ptrPainting->getDimensions();
	projection.SetOrtho(0,size.width,0,size.height,-1.0f, 1.0f);

	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1f(shader->getUniformLocation("opacity"), 1.0f);
	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"), 1, GL_FALSE, projection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,tex->texId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	/// render image
	CZUtil::drawRect(rect,trans);

	shader->end();

	fbo.end();

	delete tex;

	CZCheckGLError();
}

/// 设置转换矩阵
bool CZLayer::setTransform(CZAffineTransform &trans)
{
	if(transform == trans) return false;

	transform = trans;
	return true;
}
/// 获取转换矩阵
CZAffineTransform& CZLayer::getTransform()
{
	return transform;
}
/// 设置混合模式
bool CZLayer::setBlendMode(BlendMode &bm)
{
	if(bm == blendMode) return false;

	blendMode = bm;
	return true;
}
/// 获取混合模式
BlendMode CZLayer::getBlendMode()
{
	return blendMode;
}
/// 设置不透明度
bool CZLayer::setOpacity(float o)
{
	if(opacity == o) return false;

	opacity = CZUtil::Clamp(0.0f,1.0f,o);
	return true;
}
/// 获取不透明度
float CZLayer::getOpacity()
{
	return opacity;
}
/// 设置调整颜色
bool CZLayer::setColorBalance(CZColorBalance *cb)
{
	if(cb && *cb ==  *colorBalance) return false;
	
	colorBalance = cb;
	return true;
}
/// 设置色调调整
bool CZLayer::setHueSaturation(CZHueSaturation *hs)
{
	return true;
}
/// 设置图层图像
/// 
///		\param img - 设置的图像
///		\ret	   - 若img不为空，则设置陈宫
///		\note 调用此函数将覆盖之前对该层的所有绘制
bool CZLayer::setImage(CZImage *img)
{
	if(img == NULL)
	{
		LOG_ERROR("image is NULL\n");
		return false;
	}

	if(image) delete image;

	image = img;
    
    return true;
}

/// 撤销操作
bool CZLayer::undoAction()
{
	if (canUndo && undoFragment)
	{
		/// save redo PaintingFragment
		if (redoFragment) delete redoFragment;
		CZImage *currentImg = imageDataInRect(undoFragment->bounds);
		redoFragment = new CZPaintingFragment(currentImg,undoFragment->bounds);
		canRedo = true;

		/// take undo action
		GLint xoffset = (GLint)undoFragment->bounds.getMinX();
		GLint yoffset = (GLint)undoFragment->bounds.getMinY();
		GLsizei width = (GLsizei)undoFragment->bounds.size.width;
		GLsizei height = (GLsizei)undoFragment->bounds.size.height;

		glBindTexture(GL_TEXTURE_2D, myTexture->texId);
		glTexSubImage2D(GL_TEXTURE_2D,0,xoffset,yoffset,width,height,GL_RGBA,GL_FLOAT,undoFragment->data->data);
		canUndo = false;
	}
	else return false;

	return true;
}

/// 重做操作
bool CZLayer::redoAction()
{
	if (canRedo && redoFragment)
	{
		/// take redo action
		GLint xoffset = (GLint)redoFragment->bounds.getMinX();
		GLint yoffset = (GLint)redoFragment->bounds.getMinY();
		GLsizei width = (GLsizei)redoFragment->bounds.size.width;
		GLsizei height = (GLsizei)redoFragment->bounds.size.height;

		glBindTexture(GL_TEXTURE_2D, myTexture->texId);
		glTexSubImage2D(GL_TEXTURE_2D,0,xoffset,yoffset,width,height,GL_RGBA,GL_FLOAT,redoFragment->data->data);
		canRedo = false;
		canUndo = true;
	}
	else return false;

	return true;
}

/// 切换可见性
void CZLayer::toggleVisibility()
{
	visible = !visible;
}
/// 切换alpha锁定
void CZLayer::toggleAlphaLocked()
{
	alphaLocked = !alphaLocked;
}
/// 切换图层锁定
void CZLayer::toggleLocked()
{
	locked = !locked;
}
/// 设置可见性
void CZLayer::setVisiblility(bool flag)
{
	visible = flag;
}
/// 设置alpha锁定
void CZLayer::setAlphaLocked(bool flag)
{
	alphaLocked = flag;
}
/// 设置图层锁定
void CZLayer::setLocked(bool flag)
{
	locked = flag;
}
/// 是否被锁住图层
bool CZLayer::isLocked()
{
	return locked;
};
/// 是否被锁住alpha
bool CZLayer::isAlphaLocked()
{
	return alphaLocked;
}
/// 是否可见
bool CZLayer::isVisible()
{
	return visible;
}

/// 是否可编辑
bool CZLayer::isEditable()
{
	return (!locked && visible);
}

/// 获取编号
char *CZLayer::getUUID()
{
	return uuid;
}

/// 填充
bool CZLayer::fill(CZColor &c, CZ2DPoint &p)
{
	CZSize size = ptrPainting->getDimensions();
	if(p.x >= size.width || p.x < 0 
		|| p.y >= size.height || p.y <0)
	{
		LOG_ERROR("fill center is beyond the painting range!\n");
		return false;
	}

	/// get the original texture data
	CZImage *img = imageData();
	CZImage *inverseImg = img->modifyDataFrom((int)p.x,(int)p.y,c.red,c.green,c.blue,c.alpha,modifiedRect);
	
	/// fill the area and save undo fragment
	if(inverseImg)
	{
		ptrGLContext->setAsCurrent();
		myTexture->modifyWith(img,modifiedRect.origin.x,modifiedRect.origin.y);

		if (undoFragment) delete undoFragment;;
		undoFragment = new CZPaintingFragment(inverseImg,modifiedRect);

		canUndo = true;
	}
	
	delete img;

	return true;
}

/// 实现coding的接口
void CZLayer::update(CZDecoder *decoder_, bool deep /* = false */)
{
	/*
	// avoid setting these if they haven't changed, to prevent unnecessary notifications
	BOOL visible = [decoder decodeBooleanForKey:WDVisibleKey];
	if (visible != self.visible) {
	self.visible = visible;
	}
	BOOL locked = [decoder decodeBooleanForKey:WDLockedKey];
	if (locked != self.locked) {
	self.locked = locked;
	}
	BOOL alphaLocked = [decoder decodeBooleanForKey:WDAlphaLockedKey];
	if (alphaLocked != self.alphaLocked) {
	self.alphaLocked = alphaLocked;
	}

	WDBlendMode blendMode = (WDBlendMode) [decoder decodeIntegerForKey:WDBlendModeKey];
	blendMode = WDValidateBlendMode(blendMode);
	if (blendMode != self.blendMode) {
	self.blendMode = blendMode;
	}


	uuid_ = [decoder decodeStringForKey:WDUUIDKey];

	id opacity = [decoder decodeObjectForKey:WDOpacityKey];
	self.opacity = opacity ? [opacity floatValue] : 1.f;

	if (deep) {
	[decoder dispatch:^{
	loadedImageData_ = [[decoder decodeDataForKey:WDImageDataKey] decompress];
	self.isSaved = kWDSaveStatusSaved;
	}];
	}
	*/
}
void CZLayer::encode(CZCoder *coder_, bool deep /* = false */)
{
	/*
	[coder encodeBoolean:visible_ forKey:WDVisibleKey];
	[coder encodeBoolean:locked_ forKey:WDLockedKey];
	[coder encodeBoolean:alphaLocked_ forKey:WDAlphaLockedKey];
	[coder encodeInteger:blendMode_ forKey:WDBlendModeKey];
	[coder encodeString:uuid_ forKey:WDUUIDKey];

	if (opacity_ != 1.f) {
	[coder encodeFloat:opacity_ forKey:WDOpacityKey];
	}

	if (deep) {
	WDSaveStatus wasSaved = self.isSaved;
	self.isSaved = kWDSaveStatusTentative;
	id data = (wasSaved == kWDSaveStatusSaved) ? [NSNull null] : self.imageData; // don't bother retrieving imageData if we're not saving it
	WDTypedData *image = [WDTypedData data:data mediaType:@"image/x-brushes-layer" compress:YES uuid:self.uuid isSaved:wasSaved];
	[coder encodeDataProvider:image forKey:WDImageDataKey];
	}
	*/
}

/// 配置混合模式
void CZLayer::configureBlendMode()
{
	switch (blendMode) 
	{
	case kBlendModeMultiply:
		glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case kBlendModeScreen:
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		break;
	case kBlendModeExclusion:
		glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		break;
	default: // WDBlendModeNormal
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied blend
		break;
	}
}

/// 将纹理转换后绘制			
void CZLayer::blit(CZMat4 &projection, const CZAffineTransform &trans)
{
	// use shader program
	CZShader *shader = ptrPainting->getShader("blit");

	if (shader == NULL)
	{
		LOG_ERROR("cannot find the assigned shader!\n");
		return;
	}

	shader->begin();

	glUniformMatrix4fv(shader->getUniformLocation("mvpMat"), 1, GL_FALSE, projection);
	glUniform1i(shader->getUniformLocation("texture"), 0);
	glUniform1f(shader->getUniformLocation("opacity"), opacity);

	// Bind the texture to be used
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, myTexture->texId);

	/// 配置绘制模式
	configureBlendMode();

	CZSize size = ptrPainting->getDimensions();
	CZRect rect(0,0,size.width,size.height);

	if (clipWhenTransformed) 
	{
		glEnable(GL_STENCIL_TEST);
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT);

		// All drawing commands fail the stencil test, and are not drawn, but increment the value in the stencil buffer.
		glStencilFunc(GL_NEVER, 0, 0);
		glStencilOp(GL_INCR, GL_INCR, GL_INCR);

		CZUtil::drawRect(rect,CZAffineTransform::makeIdentity());

		// now, allow drawing, except where the stencil pattern is 0x1 and do not make any further changes to the stencil buffer
		glStencilFunc(GL_EQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	}

	CZUtil::drawRect(rect, trans);

	if (clipWhenTransformed) 
		glDisable(GL_STENCIL_TEST);

	shader->end();

	CZCheckGLError();
}

/// 注册撤销操作
void CZLayer::registerUndoInRect(CZRect &rect)
{
	if (undoFragment) delete undoFragment;
	
	CZRect newRect = rect.intersectWith(ptrPainting->getBounds());
	CZImage *currentImg = imageDataInRect(newRect);
	undoFragment = new CZPaintingFragment(currentImg,newRect);

	canUndo = true;
}
