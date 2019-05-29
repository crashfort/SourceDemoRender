#include <SDR Extension\Extension.hpp>
#include <SDR Shared\String.hpp>
#include <SDR Shared\Error.hpp>
#include <SDR Shared\Hooking.hpp>
#include <SDR Shared\D3D11.hpp>
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Table.hpp>
#include <SDR Direct2DContext API\Direct2DContextAPI.hpp>

#include <wrl.h>

#include <memory>
#include <tuple>
#include <chrono>
#include <numeric>

namespace
{
	SDR::Extension::ImportData Import;
}

namespace
{
	struct Vector3
	{
		Vector3() = default;

		Vector3(float x, float y, float z)
		{
			X = x;
			Y = y;
			Z = z;
		}

		explicit Vector3(float uniform)
		{
			X = uniform;
			Y = uniform;
			Z = uniform;
		}

		Vector3& operator+=(const Vector3& other)
		{
			X += other.X;
			Y += other.Y;
			Z += other.Z;

			return *this;
		}

		Vector3& operator-=(const Vector3& other)
		{
			X -= other.X;
			Y -= other.Y;
			Z -= other.Z;

			return *this;
		}

		Vector3 operator+(const Vector3& other) const
		{
			auto ret = *this;
			ret += other;

			return ret;
		}

		Vector3 operator-(const Vector3& other) const
		{
			auto ret = *this;
			ret -= other;

			return ret;
		}

		Vector3& operator/=(float other)
		{
			X /= other;
			Y /= other;
			Z /= other;

			return *this;
		}

		Vector3 operator/(float other) const
		{
			auto ret = *this;
			ret /= other;

			return ret;
		}

		Vector3& operator*=(float other)
		{
			X *= other;
			Y *= other;
			Z *= other;

			return *this;
		}

		Vector3 operator*(float other) const
		{
			auto ret = *this;
			ret *= other;

			return ret;
		}

		float LengthSq() const
		{
			return (X * X + Y * Y + Z * Z);
		}

		void MakeAbs()
		{
			X = std::abs(X);
			Y = std::abs(Y);
			Z = std::abs(Z);
		}

		float X;
		float Y;
		float Z;
	};
}

namespace
{
	struct VelocitySampleData
	{
		Vector3 Velocity;
	};
}

namespace
{
	namespace ModuleLocalEntity
	{
		int LocalVelocityOffset = 0;
		int AbsoluteVelocityOffset = 0;

		Vector3 GetLocalVelocity(void* ptr)
		{
			SDR::Hooking::StructureWalker walker(ptr);
			return *(Vector3*)walker.Advance(LocalVelocityOffset);
		}

		Vector3 GetAbsoluteVelocity(void* ptr)
		{
			SDR::Hooking::StructureWalker walker(ptr);
			return *(Vector3*)walker.Advance(AbsoluteVelocityOffset);
		}
	}

	namespace ModuleLocalPlayer
	{
		namespace Entries
		{
			SDR::Hooking::ModuleShared::Variant::Entry Get;
		}

		namespace Variant0
		{
			using GetType = void*(__cdecl*)();
			SDR::Hooking::ModuleShared::Variant::Function<GetType> Get(Entries::Get);
		}

		void* Get()
		{
			if (Entries::Get == 0)
			{
				return Variant0::Get()();
			}

			return nullptr;
		}
	}

	namespace ModuleSourceGeneral
	{
		namespace Entries
		{
			SDR::Hooking::ModuleShared::Variant::Entry PlayerByIndex;
			SDR::Hooking::ModuleShared::Variant::Entry GetSpectatorTarget;
		}

		namespace Variant0
		{
			using PlayerByIndexType = void*(__cdecl*)(int index);
			SDR::Hooking::ModuleShared::Variant::Function<PlayerByIndexType> PlayerByIndex(Entries::PlayerByIndex);

			using GetSpectatorTargetType = int(__cdecl*)();
			SDR::Hooking::ModuleShared::Variant::Function<GetSpectatorTargetType> GetSpectatorTarget(Entries::GetSpectatorTarget);
		}

		void* PlayerByIndex(int index)
		{
			if (Entries::PlayerByIndex == 0)
			{
				return Variant0::PlayerByIndex()(index);
			}

			return nullptr;
		}

		int GetSpectatorTarget()
		{
			if (Entries::GetSpectatorTarget == 0)
			{
				return Variant0::GetSpectatorTarget()();
			}

			return 0;
		}
	}
}

namespace
{
	struct LocalData
	{
		HRESULT CreateTextLayout(const wchar_t* text, int length)
		{
			auto width = StartMovieData.Original.Width;
			auto height = StartMovieData.Original.Height;
			auto textformat = TextFormat.Get();
			auto layout = TextLayout.ReleaseAndGetAddressOf();

			auto hr = StartMovieData.DirectWriteFactory->CreateTextLayout(text, length, textformat, width, height, layout);
			
			return hr;
		}

		void CreateVelocityText(const Vector3& vel)
		{
			auto z = std::abs(vel.Z);
			auto xy = std::sqrt(vel.X * vel.X + vel.Y * vel.Y);
			auto xyz = std::sqrt(vel.X * vel.X + vel.Y * vel.Y + vel.Z * vel.Z);

			wchar_t buffer[128];
			int length = 0;

			auto text = SDR::String::FromUTF8(UserText);
			auto textptr = text.c_str();

			switch (DisplayMode)
			{
				case DisplayModeType::Z:
				{
					length = swprintf_s(buffer, L"%s\n%0.*f", textptr, DisplayModeDecimals, z);
					break;
				}

				case DisplayModeType::XY:
				{
					length = swprintf_s(buffer, L"%s\n%0.*f", textptr, DisplayModeDecimals, xy);
					break;
				}

				case DisplayModeType::XYZ:
				{
					length = swprintf_s(buffer, L"%s\n%0.*f", textptr, DisplayModeDecimals, xyz);
					break;
				}

				case DisplayModeType::XY_Z:
				{
					length = swprintf_s(buffer, L"%s\n%0.*f %0.*f", textptr, DisplayModeDecimals, xy, DisplayModeDecimals, z);
					break;
				}
			}

			CreateTextLayout(buffer, length);

			DWRITE_TEXT_RANGE range = {};
			range.length = text.size();

			TextLayout->SetFontSize(UserTextSize, range);
		}

		Vector3 GetAverageVelocityFromSamples()
		{
			Vector3 avgvel = {};

			for (const auto& vel : VelocitySamples)
			{
				avgvel += vel.Velocity;
			}

			avgvel /= VelocitySamples.size();
			
			return avgvel;
		}

		Vector3 GetLastVelocitySample()
		{
			const auto& sample = VelocitySamples.back();
			return sample.Velocity;
		}

		void AddVelocitySample(const Vector3& velocity)
		{
			VelocitySampleData velocdata;
			velocdata.Velocity = velocity;

			VelocitySamples.emplace_back(velocdata);
		}

		void ClearVelocitySamples()
		{
			VelocitySamples.clear();
		}

		bool VelocitySamplesFull()
		{
			return VelocitySamples.size() == MaxVelocitySamples;
		}

		bool DoVelocitySampling(Vector3& velocity)
		{
			if (MaxVelocitySamples == 1)
			{
				return true;
			}

			AddVelocitySample(velocity);

			if (VelocitySamplesFull())
			{
				if (ShouldAverageSamples)
				{
					velocity = GetAverageVelocityFromSamples();
				}

				else
				{
					velocity = GetLastVelocitySample();
				}

				ClearVelocitySamples();
				return true;
			}

			return false;
		}

		bool CalculateVelocity(Vector3& velocity)
		{
			auto targetptr = ModuleLocalPlayer::Get();

			auto specid = ModuleSourceGeneral::GetSpectatorTarget();

			if (specid > 0)
			{
				auto specptr = ModuleSourceGeneral::PlayerByIndex(specid);

				if (specptr)
				{
					targetptr = specptr;
				}
			}

			switch (VelocitySource)
			{
				case VelocitySourceType::Local:
				{
					velocity = ModuleLocalEntity::GetLocalVelocity(targetptr);
					break;
				}

				case VelocitySourceType::Absolute:
				{
					velocity = ModuleLocalEntity::GetAbsoluteVelocity(targetptr);
					break;
				}
			}

			return DoVelocitySampling(velocity);
		}

		void CalculateAndCreateVelocityText()
		{
			Vector3 velocity;

			if (CalculateVelocity(velocity))
			{
				CreateVelocityText(velocity);
			}
		}

		void DrawDirect2D(const SDR::Direct2DContext::NewVideoFrameData& data)
		{
			auto drawpos = D2D1::Point2F(PositionX, PositionY);
			data.Context->DrawTextLayout(drawpos, TextLayout.Get(), TextBrush.Get());
		}

		Microsoft::WRL::ComPtr<IDWriteTextFormat> TextFormat;
		Microsoft::WRL::ComPtr<IDWriteTextLayout> TextLayout;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> TextBrush;

		int PositionX;
		int PositionY;

		SDR::Direct2DContext::StartMovieData StartMovieData;

		std::vector<VelocitySampleData> VelocitySamples;
		uint64_t MaxVelocitySamples;
		bool ShouldAverageSamples;

		enum class DisplayModeType
		{
			Z,
			XY,
			XYZ,
			XY_Z,
		};

		DisplayModeType DisplayMode;
		int DisplayModeDecimals;

		int TeleportDistance;

		std::string UserText;
		int UserTextSize;

		enum class VelocitySourceType
		{
			Local,
			Absolute,
		};

		VelocitySourceType VelocitySource;
	};

	std::unique_ptr<LocalData> LocalPtr;
}

namespace
{
	namespace Variables
	{
		uint32_t Enable;

		uint32_t FontName;
		uint32_t FontSize;
		uint32_t Locale;

		uint32_t PositionX;
		uint32_t PositionY;

		uint32_t ColorRed;
		uint32_t ColorGreen;
		uint32_t ColorBlue;
		uint32_t ColorAlpha;

		uint32_t Weight;

		uint32_t TextAlignment;
		uint32_t ParagraphAlignment;

		uint32_t Style;
		uint32_t Stretch;

		uint32_t LineSpaceMode;
		uint32_t LineSpacing;
		uint32_t BaseLineSpace;
		uint32_t LineSpaceExtra;

		uint32_t DisplayMode;
		uint32_t DisplayModeDecimals;

		uint32_t UserText;
		uint32_t UserTextSize;

		uint32_t MaxVelocitySamples;
		uint32_t ShouldAverageSamples;

		uint32_t VelocitySource;
	}
}

extern "C"
{
	__declspec(dllexport) void __cdecl SDR_Query(SDR::Extension::QueryData& query)
	{
		query.Name = "Velocity Text";
		query.Namespace = "VelocityText";
		query.Author = "crashfort";
		query.Contact = "https://github.com/crashfort/";

		query.Version = 5;

		query.Dependencies = "Direct2DContext.dll";
	}

	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::Extension::InitializeData& data)
	{
		SDR::Error::SetPrintFormat("VelocityText: %s\n");
		SDR::Extension::RedirectLogOutputs(data);
	}

	__declspec(dllexport) bool __cdecl SDR_ConfigHandler(const char* name, const rapidjson::Value& value)
	{
		try
		{
			if (SDR::String::IsEqual(name, "VelocityText_GetLocalPlayer"))
			{
				SDR::Hooking::GenericVariantInit(ModuleLocalPlayer::Entries::Get, value);
				return true;
			}

			if (SDR::String::IsEqual(name, "VelocityText_ClientEntityLocalVelocity"))
			{
				auto address = (uint8_t*)SDR::Hooking::GetAddressFromJsonPattern(value);
				ModuleLocalEntity::LocalVelocityOffset = *(int*)address;

				return true;
			}

			if (SDR::String::IsEqual(name, "VelocityText_ClientEntityAbsoluteVelocity"))
			{
				auto address = (uint8_t*)SDR::Hooking::GetAddressFromJsonPattern(value);
				ModuleLocalEntity::AbsoluteVelocityOffset = *(int*)address;

				return true;
			}

			if (SDR::String::IsEqual(name, "VelocityText_PlayerByIndex"))
			{
				SDR::Hooking::GenericVariantInit(ModuleSourceGeneral::Entries::PlayerByIndex, value);
				return true;
			}

			if (SDR::String::IsEqual(name, "VelocityText_GetSpectatorTarget"))
			{
				SDR::Hooking::GenericVariantInit(ModuleSourceGeneral::Entries::GetSpectatorTarget, value);
				return true;
			}
		}

		catch (const SDR::Error::Exception& error)
		{
			
		}

		return false;
	}

	__declspec(dllexport) void __cdecl SDR_Ready(const SDR::Extension::ImportData& data)
	{
		Import = data;

		Variables::Enable = Import.MakeBool("sdr_velocitytext_enable", "1");

		Variables::FontName = Import.MakeString("sdr_velocitytext_font", "Segoe UI");
		Variables::FontSize = Import.MakeNumber("sdr_velocitytext_size", "72");
		Variables::Locale = Import.MakeString("sdr_velocitytext_locale", "en-us");

		Variables::PositionX = Import.MakeNumber("sdr_velocitytext_posx", "100");
		Variables::PositionY = Import.MakeNumber("sdr_velocitytext_posy", "-100");

		Variables::ColorRed = Import.MakeNumberMinMax("sdr_velocitytext_color_r", "255", 0, 255);
		Variables::ColorGreen = Import.MakeNumberMinMax("sdr_velocitytext_color_g", "255", 0, 255);
		Variables::ColorBlue = Import.MakeNumberMinMax("sdr_velocitytext_color_b", "255", 0, 255);
		Variables::ColorAlpha = Import.MakeNumberMinMax("sdr_velocitytext_color_a", "150", 0, 255);

		Variables::Weight = Import.MakeString("sdr_velocitytext_weight", "thin");

		Variables::TextAlignment = Import.MakeString("sdr_velocitytext_align_text", "leading");
		Variables::ParagraphAlignment = Import.MakeString("sdr_velocitytext_align_paragraph", "far");

		Variables::Style = Import.MakeString("sdr_velocitytext_style", "normal");
		Variables::Stretch = Import.MakeString("sdr_velocitytext_stretch", "normal");

		Variables::LineSpaceMode = Import.MakeString("sdr_velocitytext_linespace_mode", "uniform");
		Variables::LineSpacing = Import.MakeNumber("sdr_velocitytext_linespace", "72");
		Variables::BaseLineSpace = Import.MakeNumber("sdr_velocitytext_linespace_base", "72");
		Variables::LineSpaceExtra = Import.MakeNumber("sdr_velocitytext_linespace_extra", "-4");

		Variables::DisplayMode = Import.MakeString("sdr_velocitytext_display", "xy");
		Variables::DisplayModeDecimals = Import.MakeNumber("sdr_velocitytext_displaydecimals", "0");

		Variables::UserText = Import.MakeString("sdr_velocitytext_usertext", "VELOCITY");
		Variables::UserTextSize = Import.MakeNumber("sdr_velocitytext_usertext_size", "36");

		Variables::MaxVelocitySamples = Import.MakeNumberMin("sdr_velocitytext_samples", "5", 1);
		Variables::ShouldAverageSamples = Import.MakeBool("sdr_velocitytext_samples_avg", "0");

		Variables::VelocitySource = Import.MakeString("sdr_velocitytext_source", "local");
	}

	__declspec(dllexport) void __cdecl Direct2DContext_StartMovie(const SDR::Direct2DContext::StartMovieData& data)
	{
		try
		{
			LocalPtr = std::make_unique<LocalData>();

			LocalPtr->StartMovieData = data;

			LocalPtr->MaxVelocitySamples = Import.GetInt(Variables::MaxVelocitySamples);
			LocalPtr->ShouldAverageSamples = Import.GetBool(Variables::ShouldAverageSamples);

			LocalPtr->DisplayMode = LocalData::DisplayModeType::XY;
			{
				auto table =
				{
					std::make_pair("z", LocalData::DisplayModeType::Z),
					std::make_pair("xy", LocalData::DisplayModeType::XY),
					std::make_pair("xyz", LocalData::DisplayModeType::XYZ),
					std::make_pair("xy z", LocalData::DisplayModeType::XY_Z),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::DisplayMode), table, LocalPtr->DisplayMode);
			}

			LocalPtr->DisplayModeDecimals = Import.GetInt(Variables::DisplayModeDecimals);

			LocalPtr->UserText = Import.GetString(Variables::UserText);
			LocalPtr->UserTextSize = Import.GetInt(Variables::UserTextSize);

			LocalPtr->VelocitySource = LocalData::VelocitySourceType::Local;
			{
				auto table =
				{
					std::make_pair("local", LocalData::VelocitySourceType::Local),
					std::make_pair("absolute", LocalData::VelocitySourceType::Absolute),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::VelocitySource), table, LocalPtr->VelocitySource);
			}

			auto weightmode = DWRITE_FONT_WEIGHT_NORMAL;
			{
				auto table =
				{
					std::make_pair("thin", DWRITE_FONT_WEIGHT_THIN),
					std::make_pair("extralight", DWRITE_FONT_WEIGHT_EXTRA_LIGHT),
					std::make_pair("light", DWRITE_FONT_WEIGHT_LIGHT),
					std::make_pair("semilight", DWRITE_FONT_WEIGHT_SEMI_LIGHT),
					std::make_pair("normal", DWRITE_FONT_WEIGHT_NORMAL),
					std::make_pair("medium", DWRITE_FONT_WEIGHT_MEDIUM),
					std::make_pair("semibold", DWRITE_FONT_WEIGHT_SEMI_BOLD),
					std::make_pair("bold", DWRITE_FONT_WEIGHT_BOLD),
					std::make_pair("extrabold", DWRITE_FONT_WEIGHT_EXTRA_BOLD),
					std::make_pair("black", DWRITE_FONT_WEIGHT_BLACK),
					std::make_pair("extrablack", DWRITE_FONT_WEIGHT_EXTRA_BLACK),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::Weight), table, weightmode);
			}

			auto style = DWRITE_FONT_STYLE_NORMAL;
			{
				auto table =
				{
					std::make_pair("normal", DWRITE_FONT_STYLE_NORMAL),
					std::make_pair("oblique", DWRITE_FONT_STYLE_OBLIQUE),
					std::make_pair("italic", DWRITE_FONT_STYLE_ITALIC),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::Style), table, style);
			}

			auto stretch = DWRITE_FONT_STRETCH_NORMAL;
			{
				auto table =
				{
					std::make_pair("undefined", DWRITE_FONT_STRETCH_UNDEFINED),
					std::make_pair("ultracondensed", DWRITE_FONT_STRETCH_ULTRA_CONDENSED),
					std::make_pair("extracondensed", DWRITE_FONT_STRETCH_EXTRA_CONDENSED),
					std::make_pair("condensed", DWRITE_FONT_STRETCH_CONDENSED),
					std::make_pair("semicondensed", DWRITE_FONT_STRETCH_SEMI_CONDENSED),
					std::make_pair("normal", DWRITE_FONT_STRETCH_NORMAL),
					std::make_pair("semiexpanded", DWRITE_FONT_STRETCH_SEMI_EXPANDED),
					std::make_pair("expanded", DWRITE_FONT_STRETCH_EXPANDED),
					std::make_pair("extraexpanded", DWRITE_FONT_STRETCH_EXTRA_EXPANDED),
					std::make_pair("ultraexpanded", DWRITE_FONT_STRETCH_ULTRA_EXPANDED),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::Stretch), table, stretch);
			}

			auto fontname = Import.GetString(Variables::FontName);
			auto fontnamewstr = SDR::String::FromUTF8(fontname);

			auto locale = Import.GetString(Variables::Locale);
			auto localewstr = SDR::String::FromUTF8(locale);

			SDR::Error::Microsoft::ThrowIfFailed
			(
				data.DirectWriteFactory->CreateTextFormat
				(
					fontnamewstr.c_str(),
					nullptr,
					weightmode,
					style,
					stretch,
					Import.GetInt(Variables::FontSize),
					localewstr.c_str(),
					LocalPtr->TextFormat.GetAddressOf()
				),
				"Could not create text format"
			);

			auto textalign = DWRITE_TEXT_ALIGNMENT_CENTER;
			{
				auto table =
				{
					std::make_pair("leading", DWRITE_TEXT_ALIGNMENT_LEADING),
					std::make_pair("trailing", DWRITE_TEXT_ALIGNMENT_TRAILING),
					std::make_pair("center", DWRITE_TEXT_ALIGNMENT_CENTER),
					std::make_pair("justified", DWRITE_TEXT_ALIGNMENT_JUSTIFIED),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::TextAlignment), table, textalign);
			}

			LocalPtr->TextFormat->SetTextAlignment(textalign);

			auto paragraphalign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
			{
				auto table =
				{
					std::make_pair("near", DWRITE_PARAGRAPH_ALIGNMENT_NEAR),
					std::make_pair("far", DWRITE_PARAGRAPH_ALIGNMENT_FAR),
					std::make_pair("center", DWRITE_PARAGRAPH_ALIGNMENT_CENTER),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::ParagraphAlignment), table, paragraphalign);
			}

			LocalPtr->TextFormat->SetParagraphAlignment(paragraphalign);

			auto red = Import.GetInt(Variables::ColorRed) / 255.0;
			auto green = Import.GetInt(Variables::ColorGreen) / 255.0;
			auto blue = Import.GetInt(Variables::ColorBlue) / 255.0;
			auto alpha = Import.GetInt(Variables::ColorAlpha) / 255.0;

			SDR::Error::Microsoft::ThrowIfFailed
			(
				data.Context->CreateSolidColorBrush
				(
					D2D1::ColorF(red, green, blue, alpha),
					LocalPtr->TextBrush.GetAddressOf()
				),
				"Could not create text brush"
			);

			auto linespace = Import.GetInt(Variables::LineSpacing);
			auto baseline = Import.GetInt(Variables::BaseLineSpace);
			auto offset = Import.GetInt(Variables::LineSpaceExtra);

			linespace += offset;
			baseline += offset;

			auto linespacemode = DWRITE_LINE_SPACING_METHOD_DEFAULT;
			{
				auto table =
				{
					std::make_pair("default", DWRITE_LINE_SPACING_METHOD_DEFAULT),
					std::make_pair("uniform", DWRITE_LINE_SPACING_METHOD_UNIFORM),
					std::make_pair("proportional", DWRITE_LINE_SPACING_METHOD_PROPORTIONAL),
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::LineSpaceMode), table, linespacemode);
			}

			LocalPtr->TextFormat->SetLineSpacing(linespacemode, linespace, baseline);

			LocalPtr->PositionX = Import.GetInt(Variables::PositionX);
			LocalPtr->PositionY = Import.GetInt(Variables::PositionY);

			LocalPtr->CreateVelocityText(Vector3(0));
		}

		catch (const SDR::Error::Exception& error)
		{
			LocalPtr.reset();
		}
	}

	__declspec(dllexport) void __cdecl SDR_EndMovie()
	{
		LocalPtr.reset();
	}

	__declspec(dllexport) void __cdecl Direct2DContext_NewVideoFrame(const SDR::Direct2DContext::NewVideoFrameData& data)
	{
		if (!Import.GetBool(Variables::Enable))
		{
			return;
		}

		if (!LocalPtr)
		{
			return;
		}

		LocalPtr->CalculateAndCreateVelocityText();
		LocalPtr->DrawDirect2D(data);
	}
}
