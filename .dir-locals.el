((nil . ((indent-tabs-mode . nil)
         (projectile-project-compilation-cmd . "ninja -C _build")
         (projectile-project-test-cmd . "ninja -C _build test")))
 ;; thanks to Mohammed Sadiq, see https://source.puri.sm/Librem5/calls/-/merge_requests/332#note_159469
 (c-mode . ((c-macro-names-with-semicolon
             . ("G_BEGIN_DECLS" "G_END_DECLS" "G_DECLARE_FINAL_TYPE" "G_DEFINE_QUARK"
                "G_DECLARE_DERIVABLE_TYPE" "G_DECLARE_INTERFACE"
                "G_DEFINE_ABSTRACT_TYPE" "G_DEFINE_ABSTRACT_TYPE_WITH_CODE"
                "G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE"
                "G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC" "G_DEFINE_AUTO_CLEANUP_FREE_FUNC"
                "G_DEFINE_AUTOPTR_CLEANUP_FUNC"
                "G_DEFINE_DYNAMIC_TYPE" "G_DEFINE_DYNAMIC_TYPE_EXTENDED"
                "G_DEFINE_INTERFACE" "G_DEFINE_INTERFACE_WITH_CODE"
                "G_DEFINE_TYPE" "G_DEFINE_TYPE_EXTENDED"
                "G_DEFINE_TYPE_WITH_CODE" "G_DEFINE_TYPE_WITH_PRIVATE"
                 )))))
