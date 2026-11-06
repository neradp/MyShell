#pragma once

#include <exception>
#include <typeinfo>
#include <stdexcept>
#include <aclapi.h>
#include <shlobj.h>
#include <initguid.h>
#include <wincodec.h> // WIC
#include <psapi.h>

#include <uxtheme.h>
#include <vssym32.h>
#include <ShellScalingApi.h>

#include <thread>
#include <mutex>
#include <functional>
#include <filesystem>
#include <map>
#include <unordered_map>

#include <d2d1.h>
#include <dwrite.h>
#include <windowsx.h>

#define maximum_path 32767
#include <System.h>

using namespace ::Nilesoft;
using namespace ::Nilesoft::Collections;
using namespace ::Nilesoft::Text;
using namespace ::Nilesoft::IO;
using namespace ::Nilesoft::Drawing;
using namespace ::Nilesoft::Windows::Forms;

#include <Resource.h>
#include <Globals.h>

using namespace Nilesoft;

#include "Parser\IdentHash.h"
#include <FileSystemObjects.h>
#include "Include\Selections.h"
#include "Parser\Token.h"
#include "Parser\Ident.h"

#include "Include\Keyboard.h"
#include "Expression\Scope.h"
#include "Expression\Context.h"
#include "Expression\ExpressionType.h"
#include "Expression\Expression.h"
#include "Expression\LiteralExpression.h"
#include "Expression\OperatorExpression.h"
#include "Expression\ArrayExpression.h"
#include "Expression\StatementExpression.h"
#include "Expression\IdentExpression.h"

#include "Include\Styles.h"
#include "Include\Menu.h"
#include "Include\MenuItem.h"
#include "Include\Cache.h"
#include "Parser\Lexer.h"
#include "Parser\Parser.h"
#include "Include\Hooker.h"
#include "Include\Initializer.h"
#include "Include\FindPattern.h"

//default 4
#define TABSIZE		1
