#include "inspectors_repo.hpp"

#include "inspectors/inspector_wcs_comlib.hpp"

#include <array>

namespace la
{
	namespace
	{
		std::array<InspectorsRepo::Registry, 1> InspectorsRegistry{ {
				//known types
				{ InspectorsCOMLib::genInspectorRegistry() },

				//any new types should be placed after this line
			} };
	}

	size_t InspectorsRepo::iterateInspectors(FlavorsRepo::Type flavor, const std::function<void(InspectorsRepo::InspectorInfo cmd)>& iterateCb)
	{
		class Context final
			: public IRegisterCtx
		{
		public:
			Context(FlavorsRepo::Type flavor, const std::function<void(InspectorsRepo::InspectorInfo inspector)>& iterateCb)
				: m_flavor{ flavor }
				, m_iterateCb{ iterateCb }
			{ }

			size_t count() const noexcept
			{
				return m_count;
			}

			FlavorsRepo::Type flavor() noexcept override
			{
				return m_flavor;
			}

			void registerInspector(InspectorInfo inspector) override
			{
				if (!inspector.executionCb)
					return;

				m_count++;
				m_iterateCb(std::move(inspector));
			}

		private:
			size_t m_count{ 0 };
			std::string_view m_tag;
			FlavorsRepo::Type m_flavor;
			const std::function<void(InspectorsRepo::InspectorInfo inspector)>& m_iterateCb;
		};

		Context ctx{ flavor, iterateCb };

		for (const auto& reg : InspectorsRegistry)
			reg.registerInspectorCb(ctx);

		return ctx.count();
	}
}
