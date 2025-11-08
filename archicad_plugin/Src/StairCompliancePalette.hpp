#ifndef STAIR_COMPLIANCE_PALETTE_HPP
#define STAIR_COMPLIANCE_PALETTE_HPP

#include "APIEnvir.h"
#include "ACAPinc.h"

#include "DGModule.hpp"

#include "StairCompliance.hpp"

class StairCompliancePalette :	public DG::Palette,
								public DG::PanelObserver,
								public DG::ListBoxObserver,
								public DG::ButtonItemObserver
{
public:
	enum Columns {
		NameColumn		= 1,
		StatusColumn	= 2,
		DetailColumn	= 3
	};

	static StairCompliancePalette&	GetInstance ();
	static GSErrCode				RegisterPalette ();
	static void						UnregisterPalette ();

	void							UpdateResults (const GS::Array<StairComplianceResult>& results,
												   const GS::UniString& summary,
												   const GS::UniString& regulation);
	void							EnsureShown ();
	void							HidePalette ();
	void							ToggleFromMenu ();

protected:
	virtual void					PanelOpened (const DG::PanelOpenEvent& ev) override;
	virtual void					PanelClosed (const DG::PanelCloseEvent& ev) override;
	virtual void					ListBoxDoubleClicked (const DG::ListBoxDoubleClickEvent& ev) override;
	virtual void					ItemToolTipRequested (const DG::ItemHelpEvent& ev, GS::UniString* toolTipText) override;
	virtual void					ButtonClicked (const DG::ButtonClickEvent& ev) override;
private:
	static GSErrCode __ACENV_CALL	PaletteCallback (Int32 referenceID, API_PaletteMessageID messageID, GS::IntPtr param);

	static StairCompliancePalette*	instance;
	static bool						paletteRegistered;

	static const GS::Guid&			PaletteGuid ();
	static Int32					PaletteReferenceId ();

	explicit						StairCompliancePalette ();
	virtual							~StairCompliancePalette ();

	static void						SetMenuItemCheckedState (bool isChecked);

	void							InitializeListBox ();
	void							FillListBox (const GS::Array<StairComplianceResult>& results);
	void							ClearListBox ();
	void							SelectResult (short listIndex) const;
	void							UpdateSummary (const GS::UniString& summary);
	void							UpdateRegulationInfo ();

	// 列宽记忆功能
	void							SaveColumnWidths ();
	void							LoadColumnWidths ();

	// 详情功能：tooltip提示
	void							SetupTooltips ();

	// PDF上传功能
	void							OnUploadPdfClicked ();
	void							ProcessPdfFile (const IO::Location& pdfLocation);

	// 手动检测功能
	void							OnCheckNowClicked ();

	DG::LeftText					summaryText;
	DG::Button						uploadPdfButton;
	DG::Button						checkNowButton;
	DG::LeftText					regulationInfoText;
	DG::SingleSelListBox			listBox;
	GS::Array<StairComplianceResult> storedResults;
	GS::Array<UIndex>				displayedRowToResult;
	static constexpr UIndex			InvalidResultIndex = static_cast<UIndex> (-1);

	// Tooltip映射: 行号 -> 规范条文完整文本
	GS::HashTable<short, GS::UniString> rowTooltips;
};

#endif
