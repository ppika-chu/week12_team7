#pragma once

#include <new>
#include <type_traits>

// 헤더툴이 해석하는 선언 표식
#define UCLASS(...)
#define UPROPERTY(...)

// 파일 식별자와 행 번호를 결합
#define UHT_BODY_JOIN_IMPL(FileId, Line) FileId##_##Line##_GENERATED_BODY
#define UHT_BODY_JOIN(FileId, Line) UHT_BODY_JOIN_IMPL(FileId, Line)

// 현재 헤더와 행 번호에 대응하는 생성 본문
#define GENERATED_BODY() UHT_BODY_JOIN(CURRENT_FILE_ID, __LINE__)