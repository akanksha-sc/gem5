; ModuleID = 'prefill.c'
source_filename = "prefill.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree norecurse nounwind
define dso_local void @prefill(i32 noundef %0, i32 noundef %1) local_unnamed_addr #0 {
  %3 = and i32 %0, 1
  %4 = icmp eq i32 %3, 0
  %5 = icmp eq i32 %1, 0
  br i1 %4, label %66, label %6

6:                                                ; preds = %2, %63
  %7 = phi i32 [ %64, %63 ], [ 0, %2 ]
  %8 = shl i32 %7, 5
  %9 = or i32 %8, 32
  br label %10

10:                                               ; preds = %47, %6
  %11 = phi i32 [ 0, %6 ], [ %61, %47 ]
  br i1 %5, label %47, label %12

12:                                               ; preds = %10
  %13 = or i32 %11, 1
  br label %14

14:                                               ; preds = %14, %12
  %15 = phi i32 [ 0, %12 ], [ %45, %14 ]
  %16 = phi i32 [ 0, %12 ], [ %38, %14 ]
  %17 = phi i32 [ 0, %12 ], [ %44, %14 ]
  %18 = phi i32 [ 0, %12 ], [ %42, %14 ]
  %19 = phi i32 [ 0, %12 ], [ %40, %14 ]
  %20 = add i32 %15, %8
  %21 = getelementptr inbounds i8, i8* inttoptr (i32 788529344 to i8*), i32 %20
  %22 = load volatile i8, i8* %21, align 1, !tbaa !8
  %23 = sext i8 %22 to i32
  %24 = add i32 %15, %9
  %25 = getelementptr inbounds i8, i8* inttoptr (i32 788529344 to i8*), i32 %24
  %26 = load volatile i8, i8* %25, align 1, !tbaa !8
  %27 = sext i8 %26 to i32
  %28 = shl i32 %15, 5
  %29 = add i32 %28, %11
  %30 = getelementptr inbounds i8, i8* inttoptr (i32 788530432 to i8*), i32 %29
  %31 = load volatile i8, i8* %30, align 2, !tbaa !8
  %32 = sext i8 %31 to i32
  %33 = add i32 %13, %28
  %34 = getelementptr inbounds i8, i8* inttoptr (i32 788530432 to i8*), i32 %33
  %35 = load volatile i8, i8* %34, align 1, !tbaa !8
  %36 = sext i8 %35 to i32
  %37 = mul nsw i32 %32, %23
  %38 = add nsw i32 %37, %16
  %39 = mul nsw i32 %36, %23
  %40 = add nsw i32 %39, %19
  %41 = mul nsw i32 %32, %27
  %42 = add nsw i32 %41, %18
  %43 = mul nsw i32 %36, %27
  %44 = add nsw i32 %43, %17
  %45 = add nuw i32 %15, 1
  %46 = icmp eq i32 %45, %1
  br i1 %46, label %47, label %14, !llvm.loop !11

47:                                               ; preds = %14, %10
  %48 = phi i32 [ 0, %10 ], [ %40, %14 ]
  %49 = phi i32 [ 0, %10 ], [ %42, %14 ]
  %50 = phi i32 [ 0, %10 ], [ %44, %14 ]
  %51 = phi i32 [ 0, %10 ], [ %38, %14 ]
  %52 = add nuw nsw i32 %11, %8
  %53 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %52
  store volatile i32 %51, i32* %53, align 8, !tbaa !14
  %54 = or i32 %11, 1
  %55 = add nuw nsw i32 %54, %8
  %56 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %55
  store volatile i32 %48, i32* %56, align 4, !tbaa !14
  %57 = add nuw nsw i32 %11, %9
  %58 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %57
  store volatile i32 %49, i32* %58, align 8, !tbaa !14
  %59 = add nuw nsw i32 %54, %9
  %60 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %59
  store volatile i32 %50, i32* %60, align 4, !tbaa !14
  %61 = add nuw nsw i32 %11, 2
  %62 = icmp ult i32 %11, 30
  br i1 %62, label %10, label %63, !llvm.loop !16

63:                                               ; preds = %47
  %64 = add nuw nsw i32 %7, 2
  %65 = icmp ult i32 %7, 30
  br i1 %65, label %6, label %126, !llvm.loop !17

66:                                               ; preds = %2, %123
  %67 = phi i32 [ %124, %123 ], [ 0, %2 ]
  %68 = shl i32 %67, 5
  %69 = or i32 %68, 32
  br label %70

70:                                               ; preds = %107, %66
  %71 = phi i32 [ 0, %66 ], [ %121, %107 ]
  br i1 %5, label %107, label %72

72:                                               ; preds = %70
  %73 = or i32 %71, 1
  br label %74

74:                                               ; preds = %74, %72
  %75 = phi i32 [ 0, %72 ], [ %105, %74 ]
  %76 = phi i32 [ 0, %72 ], [ %98, %74 ]
  %77 = phi i32 [ 0, %72 ], [ %104, %74 ]
  %78 = phi i32 [ 0, %72 ], [ %102, %74 ]
  %79 = phi i32 [ 0, %72 ], [ %100, %74 ]
  %80 = add i32 %75, %68
  %81 = getelementptr inbounds i8, i8* inttoptr (i32 788529344 to i8*), i32 %80
  %82 = load volatile i8, i8* %81, align 1, !tbaa !8
  %83 = zext i8 %82 to i32
  %84 = add i32 %75, %69
  %85 = getelementptr inbounds i8, i8* inttoptr (i32 788529344 to i8*), i32 %84
  %86 = load volatile i8, i8* %85, align 1, !tbaa !8
  %87 = zext i8 %86 to i32
  %88 = shl i32 %75, 5
  %89 = add i32 %88, %71
  %90 = getelementptr inbounds i8, i8* inttoptr (i32 788530432 to i8*), i32 %89
  %91 = load volatile i8, i8* %90, align 2, !tbaa !8
  %92 = zext i8 %91 to i32
  %93 = add i32 %73, %88
  %94 = getelementptr inbounds i8, i8* inttoptr (i32 788530432 to i8*), i32 %93
  %95 = load volatile i8, i8* %94, align 1, !tbaa !8
  %96 = zext i8 %95 to i32
  %97 = mul nuw nsw i32 %92, %83
  %98 = add nuw nsw i32 %97, %76
  %99 = mul nuw nsw i32 %96, %83
  %100 = add nuw nsw i32 %99, %79
  %101 = mul nuw nsw i32 %92, %87
  %102 = add nuw nsw i32 %101, %78
  %103 = mul nuw nsw i32 %96, %87
  %104 = add nuw nsw i32 %103, %77
  %105 = add nuw i32 %75, 1
  %106 = icmp eq i32 %105, %1
  br i1 %106, label %107, label %74, !llvm.loop !18

107:                                              ; preds = %74, %70
  %108 = phi i32 [ 0, %70 ], [ %100, %74 ]
  %109 = phi i32 [ 0, %70 ], [ %102, %74 ]
  %110 = phi i32 [ 0, %70 ], [ %104, %74 ]
  %111 = phi i32 [ 0, %70 ], [ %98, %74 ]
  %112 = add nuw nsw i32 %71, %68
  %113 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %112
  store volatile i32 %111, i32* %113, align 8, !tbaa !14
  %114 = or i32 %71, 1
  %115 = add nuw nsw i32 %114, %68
  %116 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %115
  store volatile i32 %108, i32* %116, align 4, !tbaa !14
  %117 = add nuw nsw i32 %71, %69
  %118 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %117
  store volatile i32 %109, i32* %118, align 8, !tbaa !14
  %119 = add nuw nsw i32 %114, %69
  %120 = getelementptr inbounds i32, i32* inttoptr (i32 788531520 to i32*), i32 %119
  store volatile i32 %110, i32* %120, align 4, !tbaa !14
  %121 = add nuw nsw i32 %71, 2
  %122 = icmp ult i32 %71, 30
  br i1 %122, label %70, label %123, !llvm.loop !19

123:                                              ; preds = %107
  %124 = add nuw nsw i32 %67, 2
  %125 = icmp ult i32 %67, 30
  br i1 %125, label %66, label %126, !llvm.loop !20

126:                                              ; preds = %63, %123
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
!18 = distinct !{!18, !12, !13}
!19 = distinct !{!19, !12, !13}
!20 = distinct !{!20, !12, !13}
