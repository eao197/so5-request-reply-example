require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

	required_prj 'so_5/prj_s.rb'
   target 'composable_request_reply_sample_2'

	cpp_source 'main.cpp'
}
