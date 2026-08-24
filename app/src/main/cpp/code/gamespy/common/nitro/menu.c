/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "../nonport.h"
#include "menu.h"
#include "screen.h"
#include "key.h"
#include "touch.h"

static MenuScreen Screen;

static MenuScreenConfiguration * NextMenuScreenConfiguration;

static MenuScreenConfiguration MenuExitScreenConfiguration;

static void NewMenuScreen(MenuScreenConfiguration * configuration)
{
	int count;
	
	memset(&Screen, 0, sizeof(MenuScreen));
	Screen.configuration = configuration;
	Screen.listSelection = -1;
	count = 0;
	while(configuration->choices[count].text && (count < MAX_CHOICES))
		count++;
	Screen.numChoices = count;
}

static void ShowMenuScreen(void)
{
	MenuScreenConfiguration * configuration = Screen.configuration;
	MenuScreenChoice * choices = configuration->choices;
	int options = configuration->options;
	
	const ScreenColor normalColor = SCGray;
	const ScreenColor highlightColor = SCWhite;
	const ScreenColor disabledColor = SCDarkGray;
	const int topTextLine = 10;
	const int animationLine = (topTextLine + 2);
	const int extraTextLine = (animationLine + 2);
	const int keyboardTextLine = 1;
	const int startKeyboardRowsLine = 3;
	const int numKeyboardRows = 3;
	const int numListRows = 5;
	const int startListLine = 1;
	const int startChoicesLine = (SCREEN_HEIGHT - (Screen.numChoices * 2));
	const char keyboardRows[numKeyboardRows][32] =
	{
		"1 2 3 4 5 6 7 8 9 0 - + = _ . < ",
		" A B C D E F G H I J K L M N O  ",
		"  P Q R S T U V W X Y Z  space  "
	};
	const char listBorder[32] = "--------------------";
	const int spacePos = (strchr(keyboardRows[2], 's') - keyboardRows[2]);
	const char animationChars[] = "-*";
	const gsi_time animationTickTime = 500;
	
	BOOL done = FALSE;
	int choiceIndex;
	BOOL keyboard;
	BOOL touching;
	BOOL list;
	BOOL animated;
	int x, y;
	int touchingLine;
	int touchingPos;
	int choiceLine;
	int keyboardLine;
	BOOL wasTouching = FALSE;
	int wasTouchingChoice = -1;
	ScreenColor color;
	int i;
	int range;
	char touchingChar;
	char wasTouchingChar = 0;
	int keyboardPos;
	int numListItems;
	int listScroll = 0;
	BOOL wasTouchingUp = FALSE;
	BOOL wasTouchingDown = FALSE;
	int listItem;
	const char * choiceSelection;
	int animationCount = 0;
	gsi_time lastAnimationTime = current_time();
	gsi_time now;
	char animationText[] = "-";

	animated = (options & SCREEN_OPTION_ANIMATED)?TRUE:FALSE;
	
	keyboard = (options & SCREEN_OPTION_KEYBOARD)?TRUE:FALSE;

	list = (options & SCREEN_OPTION_LIST)?TRUE:FALSE;

	if(keyboard && list)
		OS_Panic("Can't have a keyboard and a list on a menu screen\n");

	if(configuration->initFunc)
	{
		configuration->initFunc();
		if(NextMenuScreenConfiguration)
			return;
	}

	if(keyboard)
		keyboardPos = (int)strlen(Screen.keyboardText);

	while(!done)
	{
		SVC_WaitVBlankIntr();

		if(configuration->thinkFunc)
		{
			configuration->thinkFunc();
			if(NextMenuScreenConfiguration)
				return;
		}

		if(list)
		{
			for(numListItems = 0 ; numListItems < MAX_LIST_STRINGS ; numListItems++)
			{
				if(Screen.list[numListItems][0] == '\0')
					break;
			}
		}

		touching = GetTouch(&x, &y);
		if(touching)
		{
			touchingLine = (y / (TOUCH_Y_RANGE / SCREEN_HEIGHT));
			
			touchingPos = (x / (TOUCH_X_RANGE / SCREEN_WIDTH));
		}
		else if (wasTouching)
		{
			if(wasTouchingChoice != -1)
			{
				choiceSelection = choices[wasTouchingChoice].text;
				wasTouchingChoice = -1;

				if(configuration->choseFunc)
				{
					configuration->choseFunc(choiceSelection);
					if(NextMenuScreenConfiguration)
						return;
				}
			}
			else if(wasTouchingUp)
			{
				if(listScroll > 0)
					listScroll--;
			}
			else if(wasTouchingDown)
			{
				if(listScroll < (numListItems - numListRows))
					listScroll++;
			}
			else if(wasTouchingChar != 0)
			{
				if(wasTouchingChar == '<')
				{
					if(keyboardPos > 0)
					{
						keyboardPos--;
						Screen.keyboardText[keyboardPos] = '\0';
					}
				}
				else
				{
					if(keyboardPos < MAX_KEYBOARD_TEXT_LEN)
					{
						Screen.keyboardText[keyboardPos] = wasTouchingChar;
						keyboardPos++;
						Screen.keyboardText[keyboardPos] = '\0';
					}
				}
			}
		}
		wasTouching = touching;
		
		ClearScreens();
		
		SetTopScreenLineCentered(topTextLine, SCYellow, configuration->topScreenText);

		if(animated)
		{
			now = current_time();
			if((now - lastAnimationTime) >= animationTickTime)
			{
				animationCount++;
				animationCount %= strlen(animationChars);
				lastAnimationTime += animationTickTime;
			}

			animationText[0] = animationChars[animationCount];

			SetTopScreenLineCentered(animationLine, SCYellow, animationText);
		}

		for(i = 0 ; i < MAX_EXTRA_TEXT_STRINGS ; i++)
		{
			if(options & SCREEN_OPTION_EXTRAS_CENTERED)
				SetTopScreenLineCentered(extraTextLine + i, SCWhite, Screen.extraText[i]);
			else
				SetTopScreenLine(extraTextLine + i, SCWhite, Screen.extraText[i]);
		}
		
		if(list)
		{
			wasTouchingUp = FALSE;
			wasTouchingDown = FALSE;
			
			if(listScroll > 0)
			{
				if(touching && (touchingLine == startListLine))
				{
					wasTouchingUp = TRUE;
					color = highlightColor;
				}
				else
				{
					color = normalColor;
				}
				SetBottomScreenLineCentered(startListLine, color, "up");
			}
			
			SetBottomScreenLineCentered(startListLine + 1, normalColor, listBorder);
			
			for(i = 0 ; i < numListRows ; i++)
			{
				listItem = (listScroll + i);
				
				if(listItem >= numListItems)
					break;
				
				if(touching && (touchingLine == (startListLine + 2 + i)))
					Screen.listSelection = listItem;
				
				if(Screen.listSelection == listItem)
					color = highlightColor;
				else
					color = normalColor;
				
				SetBottomScreenLineCentered(startListLine + 2 + i, color, Screen.list[listScroll + i]);
			}
			
			SetBottomScreenLineCentered(startListLine + numListRows + 2, normalColor, listBorder);
			
			if(listScroll < (numListItems - numListRows))
			{
				if(touching && (touchingLine == (startListLine + numListRows + 3)))
				{
					wasTouchingDown = TRUE;
					color = highlightColor;
				}
				else
				{
					color = normalColor;
				}
				SetBottomScreenLineCentered(startListLine + numListRows + 3, color, "down");
			}
		}
		
		if(keyboard)
		{
			SetBottomScreenLine(keyboardTextLine, SCGreen, Screen.keyboardText);
			
			wasTouchingChar = 0;
			
			for(i = 0 ; i < numKeyboardRows ; i++)
			{
				keyboardLine = (startKeyboardRowsLine + i);
				
				if(touching && (touchingLine == keyboardLine))
				{
					touchingChar = keyboardRows[i][touchingPos];
					
					if(islower(touchingChar))
					{
						wasTouchingChar = ' ';
						
						touchingPos = spacePos;
						range = 5;
					}
					else
					{
						if(touchingChar != ' ')
							wasTouchingChar = touchingChar;
						
						range = 1;
					}
					
					SetBottomScreenLineHighlight(
						keyboardLine, normalColor, keyboardRows[i],
						touchingPos, range, highlightColor);
				}
				else
				{
					SetBottomScreenLine(keyboardLine, normalColor, keyboardRows[i]);
				}
			}
		}
		
		wasTouchingChoice = -1;
		for(choiceIndex = 0 ; choiceIndex < Screen.numChoices ; choiceIndex++)
		{
			choiceLine = (startChoicesLine + (choiceIndex * 2));
			
			if((choices[choiceIndex].options & CHOICE_OPTION_DISABLED) || 
				((choices[choiceIndex].options & CHOICE_OPTION_NEEDS_LIST_SELECTION) && (Screen.listSelection == -1)))
			{
				color = disabledColor;
			}
			else if(touching && (touchingLine == choiceLine))
			{
				color = highlightColor;
				wasTouchingChoice = choiceIndex;
			}
			else
			{
				color = normalColor;
			}
			
			SetBottomScreenLineCentered(choiceLine, color, choices[choiceIndex].text);
		}
	}
}

void StartMenuScreen(MenuScreenConfiguration * configuration)
{
	assert(configuration != NULL);

	SetNextMenuScreen(configuration);

	while(NextMenuScreenConfiguration != &MenuExitScreenConfiguration)
	{
		NewMenuScreen(NextMenuScreenConfiguration);

		SetNextMenuScreen(NULL);

		ShowMenuScreen();

		ClearScreens();
	}
}

void SetNextMenuScreen(MenuScreenConfiguration * configuration)
{
	NextMenuScreenConfiguration = configuration;
}

void ExitMenu(void)
{
	SetNextMenuScreen(&MenuExitScreenConfiguration);
}

MenuScreen * GetMenuScreen(void)
{
	return &Screen;
}