; ModuleID = 'top.c'
source_filename = "top.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree norecurse nounwind
define dso_local void @top(i64 noundef %0, i64 noundef %1, i64 noundef %2) local_unnamed_addr #0 {
  store volatile i64 %0, i64* inttoptr (i32 788529153 to i64*), align 8, !tbaa !8
  store volatile i64 788529344, i64* inttoptr (i32 788529161 to i64*), align 8, !tbaa !8
  store volatile i32 1024, i32* inttoptr (i32 788529169 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  br label %4

4:                                                ; preds = %4, %3
  %5 = load volatile i8, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  %6 = and i8 %5, 4
  %7 = icmp eq i8 %6, 0
  br i1 %7, label %4, label %8, !llvm.loop !15

8:                                                ; preds = %4
  store volatile i64 %1, i64* inttoptr (i32 788529153 to i64*), align 8, !tbaa !8
  store volatile i64 788530432, i64* inttoptr (i32 788529161 to i64*), align 8, !tbaa !8
  store volatile i32 1024, i32* inttoptr (i32 788529169 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  br label %9

9:                                                ; preds = %9, %8
  %10 = load volatile i8, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  %11 = and i8 %10, 4
  %12 = icmp eq i8 %11, 0
  br i1 %12, label %9, label %13, !llvm.loop !18

13:                                               ; preds = %9
  store volatile i8 1, i8* inttoptr (i32 788529280 to i8*), align 128, !tbaa !14
  br label %14

14:                                               ; preds = %14, %13
  %15 = load volatile i8, i8* inttoptr (i32 788529280 to i8*), align 128, !tbaa !14
  %16 = and i8 %15, 4
  %17 = icmp eq i8 %16, 0
  br i1 %17, label %14, label %18, !llvm.loop !19

18:                                               ; preds = %14
  store volatile i64 788531520, i64* inttoptr (i32 788529153 to i64*), align 8, !tbaa !8
  store volatile i64 %2, i64* inttoptr (i32 788529161 to i64*), align 8, !tbaa !8
  store volatile i32 4096, i32* inttoptr (i32 788529169 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  br label %19

19:                                               ; preds = %19, %18
  %20 = load volatile i8, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  %21 = and i8 %20, 4
  %22 = icmp eq i8 %21, 0
  br i1 %22, label %19, label %23, !llvm.loop !20

23:                                               ; preds = %19
  ret void
}

attributes #0 = { nofree norecurse nounwind "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+armv7-a,+dsp,+soft-float,+strict-align,-aes,-bf16,-d32,-dotprod,-fp-armv8,-fp-armv8d16,-fp-armv8d16sp,-fp-armv8sp,-fp16,-fp16fml,-fp64,-fpregs,-fullfp16,-mve,-mve.fp,-neon,-sha2,-thumb-mode,-vfp2,-vfp2sp,-vfp3,-vfp3d16,-vfp3d16sp,-vfp3sp,-vfp4,-vfp4d16,-vfp4d16sp,-vfp4sp" "use-soft-float"="true" }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"min_enum_size", i32 4}
!2 = !{i32 1, !"branch-target-enforcement", i32 0}
!3 = !{i32 1, !"sign-return-address", i32 0}
!4 = !{i32 1, !"sign-return-address-all", i32 0}
!5 = !{i32 1, !"sign-return-address-with-bkey", i32 0}
!6 = !{i32 7, !"frame-pointer", i32 2}
!7 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!8 = !{!9, !9, i64 0}
!9 = !{!"long long", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C/C++ TBAA"}
!12 = !{!13, !13, i64 0}
!13 = !{!"int", !10, i64 0}
!14 = !{!10, !10, i64 0}
!15 = distinct !{!15, !16, !17}
!16 = !{!"llvm.loop.mustprogress"}
!17 = !{!"llvm.loop.unroll.disable"}
!18 = distinct !{!18, !16, !17}
!19 = distinct !{!19, !16, !17}
!20 = distinct !{!20, !16, !17}
