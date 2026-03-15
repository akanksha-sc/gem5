; ModuleID = 'prefill.c'
source_filename = "prefill.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree norecurse nounwind
define dso_local void @prefill() local_unnamed_addr #0 {
  br label %1

1:                                                ; preds = %0, %28
  %2 = phi i32 [ 0, %0 ], [ %29, %28 ]
  %3 = shl i32 %2, 5
  %4 = shl i32 %2, 5
  br label %5

5:                                                ; preds = %1, %23
  %6 = phi i32 [ 0, %1 ], [ %26, %23 ]
  br label %7

7:                                                ; preds = %5, %7
  %8 = phi i32 [ 0, %5 ], [ %20, %7 ]
  %9 = phi i32 [ 0, %5 ], [ %21, %7 ]
  %10 = add nuw nsw i32 %9, %3
  %11 = getelementptr inbounds i8, i8* inttoptr (i32 788529344 to i8*), i32 %10
  %12 = load volatile i8, i8* %11, align 1, !tbaa !8
  %13 = sext i8 %12 to i32
  %14 = shl i32 %9, 5
  %15 = add nuw nsw i32 %14, %6
  %16 = getelementptr inbounds i8, i8* inttoptr (i32 788530432 to i8*), i32 %15
  %17 = load volatile i8, i8* %16, align 1, !tbaa !8
  %18 = sext i8 %17 to i32
  %19 = mul nsw i32 %18, %13
  %20 = add nsw i32 %19, %8
  %21 = add nuw nsw i32 %9, 1
  %22 = icmp eq i32 %21, 32
  br i1 %22, label %23, label %7, !llvm.loop !11

23:                                               ; preds = %7
  %24 = add nuw nsw i32 %6, %4
  %25 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %24
  store volatile i32 %20, i32* %25, align 4, !tbaa !14
  %26 = add nuw nsw i32 %6, 1
  %27 = icmp eq i32 %26, 32
  br i1 %27, label %28, label %5, !llvm.loop !16

28:                                               ; preds = %23
  %29 = add nuw nsw i32 %2, 1
  %30 = icmp eq i32 %29, 32
  br i1 %30, label %31, label %1, !llvm.loop !17

31:                                               ; preds = %28
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
!9 = !{!"omnipotent char", !10, i64 0}
!10 = !{!"Simple C/C++ TBAA"}
!11 = distinct !{!11, !12, !13}
!12 = !{!"llvm.loop.mustprogress"}
!13 = !{!"llvm.loop.unroll.disable"}
!14 = !{!15, !15, i64 0}
!15 = !{!"int", !9, i64 0}
!16 = distinct !{!16, !12, !13}
!17 = distinct !{!17, !12, !13}
