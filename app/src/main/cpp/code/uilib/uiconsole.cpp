
#include "ui_local.h"

Event W_Console_ChildSizeChanged
(
	"_console_childsizechanged",
	EV_DEFAULT,
	NULL,
	NULL,
	"Signal that the child area of the floating window has changed"
);

Event EV_Console_Print
(
	"print",
	EV_DEFAULT,
	"s",
	"string",
	"Print the string to the console"
);

Event EV_Console_Clear
(
	"clear",
	EV_DEFAULT,
	NULL,
	NULL,
	"Clears the console."
);

Event EV_Console_NotInteractive
(
	"notinteractive",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes this console not interactive."
);

Event EV_Console_OnlyDrawText
(
	"onlyDrawText",
	EV_DEFAULT,
	NULL,
	NULL,
	"Makes the console only draw the text."
);

Event EV_Console_MessageMode
(
	"messageMode",
	EV_DEFAULT,
	"s",
	"messageModeName",
	"Sets the message mode of the chat console."
);

Event EV_Console_RemoveLastLine
(
	"removeLastLine",
	EV_CODEONLY,
	NULL,
	NULL,
	"Removes the last line from the console."
);

CLASS_DECLARATION(UIWidget, UIConsole, NULL) {
	{&W_SizeChanged,                 &UIConsole::OnSizeChanged },
	{&EV_Console_Print,              &UIConsole::Print         },
	{&EV_Console_Clear,              &UIConsole::Clear         },
	{&EV_Console_NotInteractive,     &UIConsole::NotInteractive},
	{&EV_Console_OnlyDrawText,       &UIConsole::onlyDrawText  },
	{NULL,                           NULL                      }
};

UIConsole::UIConsole()
{
    int i;

    m_scroll                  = NULL;
    m_historyposition         = NULL;
    m_refreshcompletionbuffer = true;
    m_consolehandler          = NULL;

    m_buffersize = UI_GetCvarInt("ui_consoleLineHistory", MAX_CONSOLE_ITEMS);
    if (m_buffersize <= 0) {
        m_buffersize = MAX_CONSOLE_ITEMS;
    }

    m_items = new item[m_buffersize];

    for (i = 0; i < m_buffersize; i++) {
        m_items[i].lines = 0;
    }

    m_firstitem = 0;
    m_numitems  = 0;
    m_caret     = 0;

    m_scroll = new UIVertScroll();
    AllowActivate(true);

    m_notInteractive = false;
}

UIConsole::~UIConsole()
{
    delete[] m_items;
}

void UIConsole::NotInteractive(Event *ev)
{
	if (m_scroll) {
		m_scroll->PostEvent(new Event(EV_Widget_Hide), 0);
	}

	m_notInteractive = true;
	AllowActivate(false);
}

// Retail @0x000572F0 (116 B) writes NO UIConsole field, and it does NOT clone
void UIConsole::onlyDrawText(Event *ev)
{
	if (m_scroll) {
		m_scroll->PostEvent(new Event(EV_Widget_Hide), 0);
	}
}

int UIConsole::getFirstItem(void)
{
	return m_firstitem;
}

int UIConsole::getNextItem(int prev)
{
	if (prev == -1) {
		return -1;
	}

	prev = prev + 1;

	if (m_numitems < m_buffersize) {
		if (prev > m_firstitem) {
			if (prev - m_firstitem >= m_numitems) {
				return -1;
			}
		} else if (m_firstitem + m_numitems >= prev + m_buffersize) {
			return -1;
		}
	}

	if (prev >= m_buffersize) {
		prev = 0;
	}

	if (prev != m_firstitem) {
		return prev;
	}

	return -1;
}

int UIConsole::getLastItem(void)
{
	int item = m_firstitem + m_numitems;

	if (item - 1 >= m_buffersize) {
		return item - (m_buffersize + 1);
	}

	return item - 1;
}

int UIConsole::AddLine(void)
{
	int line = getLastItem() + 1;

	if (line >= m_buffersize) {
		line = 0;
	}

	if (m_numitems < m_buffersize) {
		m_numitems++;
	} else {
		m_firstitem++;
		if (m_firstitem >= m_buffersize) {
			m_firstitem = 0;
		}
	}

	assert(line < m_buffersize);
	return line;
}

void UIConsole::DrawBottomLine(void)
{
	int        promptwidth;
	int        top;
	float      topScaled;
	int        iXPos;
	int        iMaxStringWidth;
	int        widthbeforecaret;

	str prompt = getPrompt();

	promptwidth = m_font->getWidth(prompt.c_str(), -1, false);
	top         = m_frame.size.height - m_font->getHeight(false);
	topScaled   = top;
	m_font->Print(0.0, topScaled, prompt.c_str(), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

	iXPos           = promptwidth + 1;
	iMaxStringWidth = m_frame.size.width - iXPos - m_scroll->getSize().width;

	if (m_caret > m_currentline.length()) {
		m_caret = m_currentline.length();
	}

	if (m_caret < 0) {
		m_caret = 0;
	}

	widthbeforecaret = promptwidth + m_font->getWidth(m_currentline.c_str(), m_caret, false);

	if (m_refreshcompletionbuffer || m_completionbuffer.length() >= m_currentline.length()) {
		static const char *indicator = "...";
		const char        *pString;
		int                indicatorwidth;
		int                iChar;
		int                iCharLength;
		int                iStringLength;

		if (widthbeforecaret < iMaxStringWidth) {
			m_font->Print(iXPos, topScaled, m_currentline.c_str(), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
		} else {
			indicatorwidth = m_font->getWidth(indicator, -1, false);
			m_font->Print(iXPos, topScaled, indicator, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

			iXPos += indicatorwidth;
			iMaxStringWidth -= indicatorwidth;

			if (m_caret > 0) {
				pString       = &m_currentline[m_caret - 1];
				iStringLength = 0;
				iCharLength   = m_font->getCharWidth(pString[0], false);

				for (iChar = m_caret; iChar > 1 && iStringLength + iCharLength < iMaxStringWidth; iChar--) {
					iCharLength = m_font->getCharWidth(*pString, false);
					iStringLength += iCharLength;
					pString--;
				}

				m_font->Print(iXPos, topScaled, pString + 1, -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
				widthbeforecaret = iXPos + m_font->getWidth(pString + 1, m_caret - iChar, false);
			}
		}
	} else {
		int completewidth;
		int linewidth;

		completewidth = m_font->getWidth(m_completionbuffer.c_str(), -1, false);
		linewidth     = m_font->getWidth(&m_currentline[m_completionbuffer.length()], -1, false);
		m_font->Print(iXPos, topScaled, m_completionbuffer.c_str(), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

		iXPos += completewidth;
		m_font->Print(iXPos, topScaled, &m_currentline[m_completionbuffer.length()], -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

		DrawBoxWithSolidBorder(
			UIRect2D(
				iXPos,
				top,
				linewidth,
				m_font->getHeight(false)
			),
			UBlack,
			URed,
			1,
			2,
			1.0
		);
	}

	if ((uid.time % 750) > 375 && IsActive()) {
		DrawBox(
			UIRect2D(widthbeforecaret, top, 1.0, m_font->getHeight(false)),
			m_foreground_color,
			1.0
		);
	}
}

void UIConsole::AddHistory(void)
{
	if (m_history.IterateFromTail()) {
		if (m_history.getCurrent() != m_currentline) {
			m_history.AddTail(m_currentline);
		}
	} else {
		m_history.AddTail(m_currentline);
	}

	while (m_history.getCount() > 20) {
		m_history.IterateFromHead();
		m_history.RemoveCurrentSetNext();
	}

	m_historyposition = NULL;
}

void UIConsole::Print(Event *ev)
{
	AddText(ev->GetString(1), NULL);
}

void UIConsole::KeyEnter(void)
{
	AddHistory();
	AddText(str(">") + m_currentline + "\n", NULL);

	if (m_consolehandler) {
		m_consolehandler((m_currentline + "\n").c_str());
	}

	m_currentline = "";
	m_caret       = 0;
}

void UIConsole::setConsoleHandler(consoleHandler_t handler)
{
	m_consolehandler = handler;
}

bool UIConsole::getItemData(int index, str& string, int& color)
{
	item *theItem;

	if (index < 0 || index >= m_numitems) {
		return false;
	}

	theItem = &m_items[index];

	string = theItem->string;

	color = theItem->pColor != NULL;

	return true;
}

int UIConsole::getItemCount(void) const
{
	return m_numitems;
}

str UIConsole::getPrompt(void)
{
	static bool  built = false;
	static str   prompt;

	if (!built) {
		prompt = ">";
		built  = true;
	}

	return prompt;
}

bool UIConsole::shouldDrawBottomLine(void)
{
	return !m_notInteractive;
}
void UIConsole::AddText(const char *text, const UColor *pColor)
{
	char  converted[0x1000];
	bool  atbottom;
	int   newtop;
	int   i, size;
	item *curitem;

	uii.SR_ConvertStringResourceIds(text, converted, sizeof(converted));
	text = converted;

	atbottom = m_scroll->getPageHeight() + m_scroll->getTopItem() >= m_scroll->getNumItems();
	newtop   = m_scroll->getTopItem();

	if (!m_numitems) {
		m_numitems                  = 1;
		m_items[m_firstitem].string = "";
		m_items[m_firstitem].lines  = 0;
		m_items[m_firstitem].pColor = &m_foreground_color;
	}

	curitem = &m_items[getLastItem()];
	if (!pColor) {
		pColor = &m_foreground_color;
	}

	for (i = 0; text[i]; i++) {
		if (text[i] == '\n') {
			curitem->pColor = pColor;
			CalcLineBreaks(*curitem);

			int line = AddLine();
			curitem  = &m_items[line];
			newtop -= curitem->lines;
			curitem->lines  = 0;
			curitem->string = "";
		} else {
			curitem->string += text[i];
		}
	}

	curitem->pColor = pColor;
	CalcLineBreaks(*curitem);

	size = 0;
	for (i = m_firstitem; i != -1; i = getNextItem(i)) {
		size += m_items[i].lines;
	}

	m_scroll->setNumItems(size);

	if (atbottom || newtop > m_scroll->getNumItems() - m_scroll->getPageHeight()) {
		m_scroll->setTopItem(m_scroll->getNumItems() - m_scroll->getPageHeight());
	} else {
		m_scroll->setTopItem(newtop);
	}

	if (m_scroll->getTopItem() < 0) {
		m_scroll->setTopItem(0);
	}

	if (curitem->lines == 9) {
		AddText("\n", NULL);
	}
}

void UIConsole::CalcLineBreaks(item& theItem)
{
	int         i;
	str         sofar, checking;
	int         lensofar, lenchecking, len_of_space;
	const char *remaining;

	lensofar      = 0;
	lenchecking   = 0;
	len_of_space  = m_font->getCharWidth(' ', false);
	theItem.lines = 0;
	remaining     = theItem.string.c_str();

	for (i = 0; remaining[i]; i++) {
		if (remaining[i] == ' ') {
			sofar += checking + ' ';
			checking = "";
			lensofar += lenchecking + len_of_space;
			lenchecking = 0;
		} else {
			int charlen = m_font->getCharWidth(remaining[i], false);
			if (m_frame.size.width - m_scroll->getSize().width < (charlen + lenchecking + lensofar)) {
				if (lensofar) {
					lensofar                      = lenchecking;
					theItem.breaks[theItem.lines] = sofar.length();
					sofar                         = checking;
					lenchecking                   = charlen;
					checking                      = remaining[i];
				} else {
					lensofar                      = charlen;
					theItem.breaks[theItem.lines] = checking.length();
					sofar                         = remaining[i];
					lenchecking                   = 0;
					checking                      = "";
				}

				theItem.lines++;

				if (theItem.lines == ARRAY_LEN(theItem.breaks) - 1) {
					break;
				}
			} else {
				checking += remaining[i];
				lenchecking += charlen;
			}
		}
	}

	if (remaining[i] || lenchecking || lensofar) {
		theItem.breaks[theItem.lines++] = -1;
	}

	lensofar = 0;
	for (i = 0; i < theItem.lines; i++) {
		theItem.begins[i] = lensofar;
		if (theItem.breaks[i] != -1) {
			lensofar += theItem.breaks[i];
		}
	}
}

void UIConsole::Clear(Event *ev)
{
	Clear();
}

void UIConsole::Clear(void)
{
	int i;

	for (i = 0; i < m_buffersize; i++) {
		m_items[i].lines = 0;
	}

	m_numitems    = 0;
	m_firstitem   = 0;
	m_caret       = 0;
	m_currentline = "";

	if (m_scroll) {
		m_scroll->setNumItems(0);
	}
}

void UIConsole::FrameInitialized(void)
{
	Connect(this, W_SizeChanged, W_SizeChanged);
	OnSizeChanged(NULL);
}

void UIConsole::Draw(void)
{
    UIWidget::Draw();

	const UColor *pCurrColor;

	m_font->setColor(m_foreground_color);
	pCurrColor = &m_foreground_color;

	m_scroll->setSolidBorderColor(m_background_color);
	m_scroll->setBackgroundColor(m_background_color, WIDGET_STATE_NORMAL, true);
	m_scroll->setBackgroundAlpha(m_alpha);
	m_scroll->setForegroundColor(m_foreground_color);

	if (m_numitems) {
		int i, item;
		int top;
		int lines_drawn, at_line;
		int topitem;

		top     = (int)(m_frame.size.height - m_font->getHeight(false) * (m_scroll->getPageHeight() + 2));
		item    = m_firstitem;
		topitem = m_scroll->getTopItem() - 1;

		if (item == -1) {
			return;
		}

		for (i = m_items[item].lines; i < topitem; i += m_items[item].lines) {
			item = getNextItem(item);
			if (item == -1) {
				return;
			}
		}

		if (item == -1) {
			return;
		}

		at_line     = m_items[item].lines - (i - topitem);
		lines_drawn = 0;
		while (lines_drawn < m_scroll->getPageHeight() + 1) {
			while (at_line >= m_items[item].lines) {
				item    = getNextItem(item);
				at_line = 0;
				if (item == -1) {
					break;
				}
			}

			if (item == -1) {
				break;
			}

			if (at_line >= 0) {
				if (at_line >= m_items[item].lines) {
					break;
				}

				const ::item *theItem = &m_items[item];
				if (theItem->pColor != pCurrColor) {
					m_font->setColor(*theItem->pColor);
					pCurrColor = theItem->pColor;
				}

				m_font->setAlpha(m_alpha);
				m_font->Print(
					0.0,
					top,
					&theItem->string.c_str()[theItem->begins[at_line]],
					theItem->breaks[at_line], UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);
			}

			top += m_font->getHeight(false);
			lines_drawn++;
			at_line++;
		}
	}

	if (pCurrColor != &m_foreground_color) {
		m_font->setColor(m_foreground_color);
	}

	if (shouldDrawBottomLine()) {
		DrawBottomLine();
	}
}

void UIConsole::CharEvent(int ch)
{
	if (ch <= 31) {
		return;
	}

	m_refreshcompletionbuffer = true;

	if (m_caret > m_currentline.length()) {
		m_caret = m_currentline.length();
	}

	if (m_caret < 0) {
		m_caret = 0;
	}

	if (m_currentline.length() == m_caret) {
		m_currentline.append(str(char(ch)));
		m_caret++;
	} else if (m_caret) {
		m_currentline = str(m_currentline, 0, m_caret) + str(char(ch)) + (m_currentline.c_str() + m_caret);
		m_caret++;
	} else {
		m_currentline = str(char(ch)) + m_currentline;
		m_caret++;
	}
}

void UIConsole::KeyEscape(void) {}

const char *UIConsole::TryToCompleteMapname(const str& partial)
{
    (void)partial;
    return "";
}

qboolean UIConsole::KeyEvent(int key, unsigned int time)
{
	const char *command;

	if (key != K_TAB && key != K_BACKSPACE) {
		m_refreshcompletionbuffer = true;
	}

	switch (key) {
	case K_TAB:
		if (m_refreshcompletionbuffer) {
			m_completionbuffer        = m_currentline;
			m_refreshcompletionbuffer = false;
			m_cntcvarnumber           = 0;
			m_cntcmdnumber            = 0;
		}

		command = uii.Cmd_CompleteCommandByNumber(m_completionbuffer.c_str(), m_cntcmdnumber++);
		if (!command) {
			command = uii.Cvar_CompleteCvarByNumber(m_completionbuffer.c_str(), m_cntcvarnumber++);
			if (!command) {
				m_currentline             = m_completionbuffer;
				m_refreshcompletionbuffer = true;
				m_caret                   = m_currentline.length();
				break;
			}
		}

		if (!command) {
			command = TryToCompleteMapname(m_completionbuffer);
		}

		if (!command) {
			m_currentline             = m_completionbuffer;
			m_refreshcompletionbuffer = true;
			m_caret                   = m_currentline.length();
			break;
		}

		m_currentline = command;
		m_currentline += ' ';
		m_caret = m_currentline.length();
		break;

	case K_ENTER:
	case K_KP_ENTER:
		KeyEnter();
		break;

	case K_ESCAPE:
		KeyEscape();
		break;

	case K_MOUSE3:
		PasteFromWindowsClipboard();
		break;

	case K_BACKSPACE:
		if (!m_caret) {
			break;
		}

		if (m_refreshcompletionbuffer) {
			m_currentline = str(m_currentline, 0, m_caret - 1) + (m_currentline.c_str() + m_caret);
			m_caret--;
		} else {
			m_currentline             = m_completionbuffer;
			m_cntcvarnumber           = 0;
			m_cntcmdnumber            = 0;
			m_refreshcompletionbuffer = true;

			m_caret = m_currentline.length();
		}
		break;

	case K_UPARROW:
		if (uii.Sys_IsKeyDown(K_CTRL)) {
			if (m_scroll->getTopItem() > 0) {
				m_scroll->setTopItem(m_scroll->getTopItem() - 1);
			}
		} else {
			if (m_history.getCount()) {
				if (m_historyposition) {
					m_history.setPosition(m_historyposition);

					if (!m_history.IteratePrev()) {
						break;
					}
				} else {
					m_history.IterateFromTail();
				}

				m_historyposition = m_history.getPosition();
				m_currentline     = m_history.getCurrent();
				m_caret           = m_currentline.length();
			}
		}
		break;

	case K_DOWNARROW:
		if (uii.Sys_IsKeyDown(K_CTRL)) {
			if (m_scroll->getTopItem() + m_scroll->getPageHeight() < m_scroll->getNumItems()) {
				m_scroll->setTopItem(m_scroll->getTopItem() + 1);
			}
		} else {
			if (m_history.getCount() && m_historyposition) {
				m_history.setPosition(m_historyposition);

				if (!m_history.IterateNext()) {
					m_currentline     = str();
					m_caret           = 0;
					m_historyposition = NULL;
					break;
				}

				m_historyposition = m_history.getPosition();
				m_currentline     = m_history.getCurrent();
				m_caret           = m_currentline.length();
			}
		}
		break;

	case K_LEFTARROW:
		if (m_caret > 0) {
			m_caret--;
		}
		break;

	case K_RIGHTARROW:
		if (m_caret < m_currentline.length()) {
			m_caret++;
		}
		break;

	case K_DEL:
		if (m_caret >= m_currentline.length()) {
			break;
		}

		m_currentline = str(m_currentline, 0, m_caret) + (m_currentline.c_str() + m_caret + 1);
		break;

	case K_PGDN:
		if (m_scroll->getNumItems() >= m_scroll->getPageHeight()) {
			unsigned int top = m_scroll->getNumItems() - m_scroll->getPageHeight();
			if ((unsigned int)(m_scroll->getPageHeight() + m_scroll->getTopItem()) < top) {
				top = m_scroll->getPageHeight() + m_scroll->getTopItem();
			}
			m_scroll->setTopItem(top);
		}
		break;

	case K_PGUP:
		if (m_scroll->getNumItems() >= m_scroll->getPageHeight()) {
			m_scroll->setTopItem(m_scroll->getTopItem() - m_scroll->getPageHeight());
		}
		break;

	case K_HOME:
		if (uii.Sys_IsKeyDown(K_CTRL)) {
			m_scroll->setTopItem(0);
		} else {
			m_caret = 0;
		}
		break;

	case K_END:
		if (uii.Sys_IsKeyDown(K_CTRL)) {
			m_scroll->setTopItem(m_scroll->getNumItems());
		} else {
			m_caret = m_currentline.length();
		}
		break;

	case K_MWHEELDOWN:
		m_scroll->AttemptScrollTo(m_scroll->getTopItem() + 2);
		break;

	case K_MWHEELUP:
		m_scroll->AttemptScrollTo(m_scroll->getTopItem() - 2);
		break;
	}

	return true;
}

void UIConsole::OnSizeChanged(Event *ev)
{
	int  linesperpage;
	int  numscroll;
	int  item;
	int  topitem;
	int  topline;
	int  atscroll;
	bool attop;
	bool atbottom;

	numscroll = 0;
	topitem   = -1;
	topline   = 0;
	atscroll  = 0;
	attop     = false;
	atbottom  = false;

	linesperpage = (m_frame.size.height / (float)m_font->getHeight(false));
	m_scroll->InitFrameAlignRight(this);

	if (ev) {
		attop    = m_scroll->getTopItem() == 0;
		atbottom = m_scroll->getPageHeight() + m_scroll->getTopItem() >= m_scroll->getNumItems();
	}

	m_scroll->setPageHeight(linesperpage - 1);
	if (m_scroll->getPageHeight() < 1) {
		m_scroll->setPageHeight(1);
	}

	for (item = m_firstitem; item != -1; item = getNextItem(item)) {
		atscroll += m_items[item].lines;
		if (atscroll >= m_scroll->getTopItem() && topitem == -1) {
			topitem = item;
		}

		CalcLineBreaks(m_items[item]);

		numscroll += m_items[item].lines;
		if (topitem == -1 || topitem == item) {
			topline += m_items[item].lines;
		}
	}

	m_scroll->setNumItems(numscroll);

	if (topitem != -1) {
		if (topline > m_scroll->getNumItems() - m_scroll->getPageHeight()) {
			atbottom = true;
		}

		if (!atbottom) {
			if (attop) {
				m_scroll->setTopItem(0);
			} else {
				if (topline < 0) {
					topline = 0;
				}

				m_scroll->setTopItem(topline);
			}
		}
	}

	m_scroll->setTopItem(m_scroll->getNumItems() - m_scroll->getPageHeight());
	if (m_scroll->getTopItem() < 0) {
		m_scroll->setTopItem(0);
	}
}

CLASS_DECLARATION(UIFloatingWindow, UIFloatingConsole, NULL) {
	{&W_Console_ChildSizeChanged,        &UIFloatingConsole::OnChildSizeChanged},
	{&UIFloatingConsole::W_ClosePressed, &UIFloatingConsole::OnClosePressed    },
	{NULL,                               NULL                                  }
};

UIFloatingConsole::UIFloatingConsole()
{
	m_status  = NULL;
	m_handler = NULL;
	setConsoleColor(UWhite);
	setConsoleBackground(UBlack, 1.0);
}

UIFloatingConsole::~UIFloatingConsole()
{
	if (m_console) {
		delete m_console;
	}
	if (m_status) {
		delete m_status;
	}
}

void UIFloatingConsole::FrameInitialized(void)
{
	UIFloatingWindow::FrameInitialized();

	m_status = new UIStatusBar(alignment_t::WND_ALIGN_BOTTOM, 20.0);
	m_status->InitFrame(
		getChildSpace(), UIRect2D(0, 0, 20.0, 20.0), 0, "tahoma-8"
	);
	m_status->EnableSizeBox(this);
	m_status->setTitle(str());

	m_console = new UIConsole();
	m_console->InitFrame(
		getChildSpace(), UIRect2D(0, 0, 20.0, 20.0), 0, "tahoma-10"
	);
	setConsoleColor(m_consoleColor);
	setConsoleBackground(m_consoleBackground, m_consoleAlpha);
	m_console->setConsoleHandler(m_handler);

	getChildSpace()->Connect(this, W_SizeChanged, W_Console_ChildSizeChanged);
	getChildSpace()->AllowActivate(false);
	OnChildSizeChanged(NULL);

	m_background_color.a = 0.0;
	m_alpha              = 1.0;

	getChildSpace()->setBackgroundAlpha(1.0);
}

void UIFloatingConsole::OnChildSizeChanged(Event *ev)
{
	const UISize2D childSpaceSize = getChildSpace()->getSize();
	const UISize2D statusSize     = m_status->getSize();
	const UISize2D newSize(childSpaceSize.width, childSpaceSize.height - statusSize.height);

	m_console->setFrame(UIRect2D(UIPoint2D(0, 0), newSize));
}

// NO call to UIWidget::ChangeResolution -- retail deliberately leaves the
void UIFloatingConsole::ChangeResolution(void)
{
	OnChildSizeChanged(NULL);
}

void UIFloatingConsole::AddText(const char *text, const UColor *pColor)
{
	if (m_console) {
		m_console->AddText(text, pColor);
	}
}

void UIFloatingConsole::setConsoleHandler(consoleHandler_t handler)
{
	if (m_console) {
		m_console->setConsoleHandler(handler);
	} else {
		m_handler = handler;
	}
}

void UIFloatingConsole::Clear(void)
{
	if (m_console) {
		m_console->Clear();
	}
}

void UIFloatingConsole::OnClosePressed(Event *ev)
{
	SendSignal(UIFloatingWindow::W_ClosePressed);
}

void UIFloatingConsole::setConsoleBackground(const UColor& color, float alpha)
{
	m_alpha             = alpha;
	m_consoleBackground = color;
	m_consoleAlpha      = alpha;

	if (m_console) {
		m_console->setBackgroundAlpha(alpha);
		m_console->setBackgroundColor(color, WIDGET_STATE_NORMAL, true);
	}

	if (m_status) {
		m_status->setBackgroundAlpha(alpha);
		m_status->setBackgroundColor(color, WIDGET_STATE_NORMAL, true);
	}
}

void UIFloatingConsole::setConsoleColor(const UColor& color)
{
	m_consoleColor = color;

	if (m_console) {
		m_console->setForegroundColor(color);
	}

	if (m_status) {
		m_status->setForegroundColor(color);
	}
}



Event EV_Minicon_Goin("_minicon_goin", EV_DEFAULT, NULL, NULL, "Event to make the miniconsole disappear");

CLASS_DECLARATION(UIWidget, Miniconsole, NULL) {
	{&W_SizeChanged,   &Miniconsole::OnSizeChanged},
	{&EV_Minicon_Goin, &Miniconsole::MoveInEvent  },
	{NULL,             NULL                       }
};

Miniconsole::Miniconsole()
{
	m_size        = UISize2D(0.0f, 0.0f);
	m_maxlines    = 0;
	m_reallyshown = true;
	m_boxstate    = box_in;
	m_boxtime     = uid.time;
	m_movespeed   = 500;
}

Miniconsole::~Miniconsole()
{
	int i;

	for (i = 1; i <= m_text.NumObjects(); i++) {
		mini_console_text_t *line = &m_text.ObjectAt(i);

		if (line && line->text) {
			delete line->text;
		}
	}
}

void Miniconsole::setShowState(void)
{
	if (m_reallyshown) {
		setShow(m_boxstate != box_in);
	} else {
		setShow(false);
	}
}

void Miniconsole::setRealShow(bool b)
{
	m_reallyshown = b;
	setShowState();
}

void Miniconsole::VerifyBoxOut(void)
{
	PostMoveinEvent();

	if (m_boxstate && m_boxstate != box_out) {
		ChangeBoxState(box_moving_out);
	}
}

void Miniconsole::Print(const char *text, int color)
{
	mini_console_text_t *lastline;
	const char          *p;
	char                 converted[0x1000];

	uii.SR_ConvertStringResourceIds(text, converted, sizeof(converted));
	text = converted;

	if (!m_text.NumObjects()) {
		mini_console_text_t newline;

		newline.text  = new str;
		newline.color = 0;
		m_text.AddObject(newline);
	}

	lastline = &m_text.ObjectAt(m_text.NumObjects());

	for (p = text; *p; p++) {
		if (*p != '\n') {
			*lastline->text += *p;
			lastline->color = color;
		} else {
			mini_console_text_t newline;

			newline.text  = new str;
			newline.color = color;
			m_text.AddObject(newline);
			lastline = &m_text.ObjectAt(m_text.NumObjects());
		}
	}

	if (lastline && lastline->text->length() > 128) {
		Print("\n", color);
	}

	if (m_maxlines < 0) {
		m_maxlines = 1;
	}

	while (m_text.NumObjects() > m_maxlines) {
		mini_console_text_t *line = &m_text.ObjectAt(1);

		if (line && line->text) {
			delete line->text;
		}

		m_text.RemoveObjectAt(1);
	}

	VerifyBoxOut();
}

void Miniconsole::PostMoveinEvent(void)
{
	if (m_boxstate != box_out) {
		return;
	}

	CancelEventsOfType(EV_Minicon_Goin);
	PostEvent(new Event(EV_Minicon_Goin), 5.f);
}

void Miniconsole::OnSizeChanged(Event *ev)
{
	m_maxlines = m_frame.size.height / m_font->getHeight(false);
}

void Miniconsole::MoveInEvent(Event *ev)
{
	m_boxstate = box_moving_in;
	m_boxtime  = uid.time;
	setShowState();
}

void Miniconsole::HandleBoxMoving(void)
{
	int       delta;
	UIRect2D  newRect;

	if (m_boxstate == box_out || m_boxstate == box_in) {
		return;
	}

	delta     = m_movespeed * (uid.time - m_boxtime) / 1000;
	m_boxtime = 1000 * delta / m_movespeed + m_boxtime;

	switch (m_boxstate) {
	case box_moving_in:
		newRect = UIRect2D(delta + m_frame.pos.x, m_frame.pos.y, m_frame.size.width, m_frame.size.height);
		if (newRect.pos.x >= uid.vidWidth) {
			newRect.pos.x = uid.vidWidth;
			m_boxstate    = box_in;
			m_boxtime     = uid.time;
			setShowState();
		}
		break;
	case box_moving_out:
		newRect = UIRect2D(-delta + m_frame.pos.x, m_frame.pos.y, m_frame.size.width, m_frame.size.height);
		if (newRect.pos.x <= uid.vidWidth - newRect.size.width) {
			newRect.pos.x = uid.vidWidth - newRect.size.width;
			m_boxstate    = box_out;
			m_boxtime     = uid.time;
			setShowState();
			PostMoveinEvent();
		}
		break;
	default:
		m_boxstate = box_in;
		m_boxtime  = uid.time;
		setShowState();
		break;
	}

	setFrame(newRect);
}

void Miniconsole::Draw(void)
{
	float aty;
	int   i;

	if (GetBackgroundMode()) {
		return;
	}

	HandleBoxMoving();

	m_font->setColor(getForegroundColor());
	aty = m_frame.size.height - m_font->getHeight(false);
	for (i = m_text.NumObjects(); i > 0; i--) {
		mini_console_text_t *line = &m_text.ObjectAt(i);

		if (-m_font->getHeight(false) >= aty) {
			break;
		}

		if (line->color) {
			m_font->setColor(UYellow);
		}

		m_font->Print(0, aty, line->text->c_str(), -1, UColor(0.0f, 0.0f, 0.0f, 0.0f), 0);

		if (line->color) {
			m_font->setColor(getForegroundColor());
		}

		aty -= m_font->getHeight(false);
	}
}

void Miniconsole::Create(const UISize2D& size, const UColor& fore, const UColor& back, float alpha)
{
	m_size = size;

	if (UI_GetCvarInt("ui_minicon", 0) == 2) {
		InitFrame(NULL, uid.vidWidth, uid.vidHeight - size.height, size.width, size.height, 0, "tahoma-8");
	} else {
		InitFrame(NULL, uid.vidWidth, 0.0f, size.width, size.height, 0, "tahoma-8");
	}

	setBackgroundColor(back, WIDGET_STATE_NORMAL, true);
	setForegroundColor(fore);
	setBackgroundAlpha(alpha);
	Connect(this, W_SizeChanged, W_SizeChanged);
	OnSizeChanged(NULL);

	m_movespeed = size.width * 3;

	setShowState();
}

void Miniconsole::ChangeBoxState(boxstate_t state)
{
	m_boxstate = state;
	m_boxtime  = uid.time;
	setShowState();
	if (state == box_out) {
		PostMoveinEvent();
	}
}

void Miniconsole::UpdateFrame(void)
{
	if (UI_GetCvarInt("ui_minicon", 0) == 2) {
		setFrame(UIRect2D(uid.vidWidth, uid.vidHeight - m_size.height, m_size.width, m_size.height));
	} else {
		setFrame(UIRect2D(uid.vidWidth, 0.0f, m_size.width, m_size.height));
	}
}




UIChatConsole::UIChatConsole()
{
    m_messageMode = 0;
    m_activeTime  = 0;
    m_activeAlpha = 1.0f;
}

str UIChatConsole::getPrompt(void)
{
    str  prompt = "^";
    char converted[0x400];

    if (m_messageMode == 2) {
        prompt += "3";
        prompt += " ";
        prompt += "($$ChatPromptTeam$$) >";
    } else {
        prompt += "5";
        prompt += " ";
        prompt += "($$ChatPromptAll$$) >";
    }

    prompt += "^";
    prompt += "8";
    prompt += " ";

    uii.SR_ConvertStringResourceIds(prompt.c_str(), converted, sizeof(converted));
    prompt = converted;

    return prompt;
}

bool UIChatConsole::shouldDrawBottomLine(void)
{
    return !m_notInteractive && m_messageMode;
}

void UIChatConsole::KeyEscape(void)
{
    if (m_currentline.length()) {
        m_currentline = "";
    }

    m_caret = 0;
    uii.ActivateView3D();

    m_activeTime  = uid.time;
    m_alpha       = m_activeAlpha;
    m_messageMode = 0;
}

void UIChatConsole::messageMode(Event *ev)
{
    str mode = ev->GetString(1);

    if (str::icmp(mode.c_str(), "team")) {
        m_messageMode = 1;
    } else {
        m_messageMode = 2;
    }
}

void UIChatConsole::removeLastLine(Event *ev)
{
    m_firstitem++;
    m_numitems--;

    if (m_firstitem >= m_buffersize) {
        m_firstitem = 0;
    }

    ProcessEvent(new Event(W_SizeChanged));
}

int UIChatConsole::AddLine(void)
{
    PostEvent(new Event(EV_Console_RemoveLastLine), 8.0f);

    return UIConsole::AddLine();
}

void UIChatConsole::KeyEnter(void)
{
    AddHistory();

    if (m_currentline.length()) {
        if (m_messageMode == 2) {
            uii.Cmd_Stuff(va("tsay \"%s\"\n", m_currentline.c_str()));
        } else {
            uii.Cmd_Stuff(va("say \"%s\"\n", m_currentline.c_str()));
        }

        m_currentline = "";
    }

    m_caret       = 0;
    m_activeTime  = uid.time;
    m_alpha       = m_activeAlpha;
    m_messageMode = 0;
}

void UIChatConsole::CharEvent(int ch)
{
    if (m_currentline.length() <= 78) {
        UIConsole::CharEvent(ch);
    }
}

// shipped table does (0x23F2D0+0xA8), not because the body differs.
void UIChatConsole::Draw(void)
{
    UIConsole::Draw();
}

void UIChatConsole::Print(Event *ev)
{
    AddText(ev->GetString(1), NULL);

    uii.Snd_PlaySound("sound/misc/menu/change.wav", CHAN_MENU);  

    m_activeTime = uid.time;
    m_alpha      = m_activeAlpha;
}

void UIChatConsole::FrameInitialized(void)
{
    Connect(this, W_SizeChanged, W_SizeChanged);
    OnSizeChanged(NULL);
}

void UIChatConsole::setShow(bool show)
{
    if (!show && m_visible) {
        m_currentline = "";
        m_caret       = 0;
        m_activeTime  = uid.time;
        m_alpha       = m_activeAlpha;
        m_messageMode = 0;
    }

    UIWidget::setShow(show);
}

CLASS_DECLARATION(UIConsole, UIChatConsole, NULL) {
    {&EV_Console_MessageMode,    &UIChatConsole::messageMode   },
    {&EV_Console_RemoveLastLine, &UIChatConsole::removeLastLine},
    {NULL,                       NULL                         }
};
