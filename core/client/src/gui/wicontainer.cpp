#include "stdafx_client.h"
#include "pragma/gui/wicontainer.h"

LINK_WGUI_TO_CLASS(WIContainer,WIContainer);

WIContainer::WIContainer()
	: WIBase(),m_padding{0,0,0,0}
{}

void WIContainer::SetPadding(int32_t padding)
{
	for(auto &v : m_padding)
		v = padding;
}
void WIContainer::SetPadding(int32_t top,int32_t right,int32_t bottom,int32_t left) {m_padding = {top,right,bottom,left};}
void WIContainer::SetPadding(Padding paddingType,int32_t padding)
{
	switch(paddingType)
	{
		case Padding::Top:
			SetPaddingTop(padding);
			break;
		case Padding::Right:
			SetPaddingRight(padding);
			break;
		case Padding::Bottom:
			SetPaddingBottom(padding);
			break;
		default:
			SetPaddingLeft(padding);
			break;
	}
}
void WIContainer::SizeToContents()
{
	WIBase::SizeToContents();
	SetSize(GetWidth() +GetPaddingLeft() +GetPaddingRight(),GetHeight() +GetPaddingTop() +GetPaddingBottom());
}
void WIContainer::SetPaddingTop(int32_t top) {m_padding[0] = top;}
void WIContainer::SetPaddingRight(int32_t right) {m_padding[1] = right;}
void WIContainer::SetPaddingBottom(int32_t bottom) {m_padding[2] = bottom;}
void WIContainer:: SetPaddingLeft(int32_t left) {m_padding[3] = left;}
const std::array<int32_t,4> &WIContainer::GetPadding() const {return m_padding;}
int32_t WIContainer::GetPaddingTop() const {return m_padding[0];}
int32_t WIContainer::GetPaddingRight() const {return m_padding[1];}
int32_t WIContainer::GetPaddingBottom() const {return m_padding[2];}
int32_t WIContainer::GetPaddingLeft() const {return m_padding[3];}
int32_t WIContainer::GetPadding(Padding paddingType) const
{
	switch(paddingType)
	{
		case Padding::Top:
			return GetPaddingTop();
		case Padding::Right:
			return GetPaddingRight();
		case Padding::Bottom:
			return GetPaddingBottom();
		default:
			return GetPaddingLeft();
	}
}