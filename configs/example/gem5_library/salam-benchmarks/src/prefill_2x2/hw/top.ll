; ModuleID = 'top.c'
source_filename = "top.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree norecurse nounwind
define dso_local void @top(i64 noundef %0, i64 noundef %1, i64 noundef %2, i32 noundef %3) local_unnamed_addr #0 {
  store volatile i64 %0, i64* inttoptr (i32 788529153 to i64*), align 8, !tbaa !8
  store volatile i64 788529344, i64* inttoptr (i32 788529161 to i64*), align 8, !tbaa !8
  store volatile i32 1024, i32* inttoptr (i32 788529169 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  br label %5

5:                                                ; preds = %5, %4
  %6 = load volatile i8, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  %7 = and i8 %6, 4
  %8 = icmp eq i8 %7, 0
  br i1 %8, label %5, label %9, !llvm.loop !15

9:                                                ; preds = %5
  store volatile i64 %1, i64* inttoptr (i32 788529153 to i64*), align 8, !tbaa !8
  store volatile i64 788530432, i64* inttoptr (i32 788529161 to i64*), align 8, !tbaa !8
  store volatile i32 1024, i32* inttoptr (i32 788529169 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  br label %10

10:                                               ; preds = %10, %9
  %11 = load volatile i8, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  %12 = and i8 %11, 4
  %13 = icmp eq i8 %12, 0
  br i1 %13, label %10, label %14, !llvm.loop !18

14:                                               ; preds = %10
  store volatile i32 %3, i32* inttoptr (i32 788529281 to i32*), align 4, !tbaa !12
  store volatile i32 32, i32* inttoptr (i32 788529285 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529280 to i8*), align 128, !tbaa !14
  br label %15

15:                                               ; preds = %15, %14
  %16 = load volatile i8, i8* inttoptr (i32 788529280 to i8*), align 128, !tbaa !14
  %17 = and i8 %16, 4
  %18 = icmp eq i8 %17, 0
  br i1 %18, label %15, label %19, !llvm.loop !19

19:                                               ; preds = %15
  store volatile i64 788531520, i64* inttoptr (i32 788529153 to i64*), align 8, !tbaa !8
  store volatile i64 %2, i64* inttoptr (i32 788529161 to i64*), align 8, !tbaa !8
  store volatile i32 4096, i32* inttoptr (i32 788529169 to i32*), align 4, !tbaa !12
  store volatile i8 1, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  br label %20

20:                                               ; preds = %20, %19
  %21 = load volatile i8, i8* inttoptr (i32 788529152 to i8*), align 16777216, !tbaa !14
  %22 = and i8 %21, 4
  %23 = icmp eq i8 %22, 0
  br i1 %23, label %20, label %24, !llvm.loop !20

24:                                               ; preds = %20
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
