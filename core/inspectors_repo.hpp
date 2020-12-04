#ifndef LA_INSPECTORS_REPO_HPP
#define LA_INSPECTORS_REPO_HPP

#include "lines_tools.hpp"
#include "flavors_repo.hpp"

#include <functional>
#include <string_view>

namespace la
{
	class InspectorsRepo
	{
	public:
		struct IResultCtx
		{
			virtual void addInfo(std::string_view ctx, std::string_view msg) = 0;
			virtual void addInfo(std::string_view ctx, std::string_view msg, size_t lineIndex) = 0;
			virtual void addInfo(std::string_view ctx, std::string_view msg, LinesTools::LineIndexRange lineRange) = 0;

			virtual void addWarning(std::string_view ctx, std::string_view msg) = 0;
			virtual void addWarning(std::string_view ctx, std::string_view msg, size_t lineIndex) = 0;
			virtual void addWarning(std::string_view ctx, std::string_view msg, LinesTools::LineIndexRange lineRange) = 0;

			virtual void addExecution(std::string_view msg, int64_t timestampStart, int64_t timestampFinish, LinesTools::LineIndexRange lineRange) = 0;
		};

		struct InspectorInfo
		{
			std::function<void(IResultCtx& inspectionCtx, const LinesTools& linesTools)> executionCb;
		};

		struct IRegisterCtx
		{
			virtual FlavorsRepo::Type flavor() noexcept = 0;
			virtual void registerInspector(InspectorInfo inspector) = 0;
		};

		struct Registry
		{
			std::function<void(IRegisterCtx& registerCtx)> registerInspectorCb;
		};

	public:
		static size_t iterateInspectors(FlavorsRepo::Type flavor, const std::function<void(InspectorInfo inspector)>& iterateCb);
	};
}

#endif
